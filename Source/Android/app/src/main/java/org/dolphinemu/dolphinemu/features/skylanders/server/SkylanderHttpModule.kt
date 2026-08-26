// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders.server

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.Base64
import org.dolphinemu.dolphinemu.features.httpserver.HttpRequest
import org.dolphinemu.dolphinemu.features.httpserver.HttpResponse
import org.dolphinemu.dolphinemu.features.httpserver.HttpRouteModule
import org.dolphinemu.dolphinemu.features.skylanders.SkylanderConfig
import org.dolphinemu.dolphinemu.features.skylanders.ui.SkylanderSlot
import org.dolphinemu.dolphinemu.utils.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream

/**
 * An [HttpRouteModule] providing REST API endpoints for Skylanders Portal emulation.
 */
class SkylanderHttpModule(
    private val context: Context,
    private val slotListSupplier: () -> List<SkylanderSlot>,
    private val onSlotUpdated: (slotIndex: Int, portalSlot: Int, name: String) -> Unit,
    private val onSlotCleared: (slotIndex: Int) -> Unit
) : HttpRouteModule {

    override val name: String = "Skylanders Portal"
    override val description: String = "Manage active Portal of Power figures in real-time."
    override val basePath: String = "/api/skylanders"
    override val webUiPath: String = "/skylanders/"

    private val mainHandler = Handler(Looper.getMainLooper())

    override fun handle(request: HttpRequest): HttpResponse? {
        return when {
            request.path == "$basePath/status" && request.method == "GET" -> {
                HttpResponse.json(getStatusJson().toString())
            }
            request.path == "$basePath/catalog" && request.method == "GET" -> {
                HttpResponse.json(getCatalogJson().toString())
            }
            request.path == "$basePath/load" && request.method == "POST" -> {
                HttpResponse.json(handleLoadRequest(request.body).toString())
            }
            request.path == "$basePath/remove" && request.method == "POST" -> {
                HttpResponse.json(handleRemoveRequest(request.body).toString())
            }
            request.path == "$basePath/upload" && request.method == "POST" -> {
                HttpResponse.json(handleUploadRequest(request.body).toString())
            }
            else -> null
        }
    }

    private fun getStatusJson(): JSONObject {
        val slots = slotListSupplier()
        val json = JSONObject()
        json.put("success", true)
        json.put("running", true)

        val slotsArray = JSONArray()
        slots.forEachIndexed { index, slot ->
            val slotObj = JSONObject()
            slotObj.put("slot", index)
            slotObj.put("portalSlot", slot.portalSlot)
            slotObj.put("name", slot.label)
            slotObj.put("occupied", slot.portalSlot != -1)
            slotsArray.put(slotObj)
        }
        json.put("slots", slotsArray)
        return json
    }

    private fun getCatalogJson(): JSONArray {
        val array = JSONArray()
        for ((pair, name) in SkylanderConfig.LIST_SKYLANDERS) {
            val item = JSONObject()
            item.put("id", pair.id)
            item.put("variant", pair.variant)
            item.put("name", name)
            array.put(item)
        }
        return array
    }

    private fun handleLoadRequest(body: String): JSONObject {
        val response = JSONObject()
        try {
            val json = if (body.isNotBlank()) JSONObject(body) else JSONObject()
            val slotIndex = json.optInt("slot", 0).coerceIn(0, 15)
            val id = json.optInt("id", -1)
            val variant = json.optInt("variant", 0)
            val name = json.optString("name", "")
            val path = json.optString("path", "")

            val slots = slotListSupplier()
            val currentSlot = if (slotIndex < slots.size) slots[slotIndex] else null
            val portalSlotToUse = currentSlot?.portalSlot ?: slotIndex

            var loadResult: android.util.Pair<Int?, String?>? = null

            if (path.isNotBlank()) {
                loadResult = SkylanderConfig.loadSkylander(portalSlotToUse, path)
            } else if (id != -1) {
                loadResult = SkylanderConfig.loadOrAutoCreate(context, id, variant, portalSlotToUse)
            } else if (name.isNotBlank()) {
                loadResult = SkylanderConfig.loadOrAutoCreateByName(context, name, portalSlotToUse)
            }

            if (loadResult != null && loadResult.first != null) {
                val loadedPortalSlot = loadResult.first!!
                val loadedName = loadResult.second ?: "Skylander"

                mainHandler.post {
                    onSlotUpdated(slotIndex, loadedPortalSlot, loadedName)
                }

                response.put("success", true)
                response.put("slot", slotIndex)
                response.put("portalSlot", loadedPortalSlot)
                response.put("name", loadedName)
            } else {
                response.put("success", false)
                response.put("error", "Failed to load figure onto portal slot $slotIndex")
            }
        } catch (e: Exception) {
            response.put("success", false)
            response.put("error", e.message ?: "Unknown error")
        }
        return response
    }

    private fun handleRemoveRequest(body: String): JSONObject {
        val response = JSONObject()
        try {
            val json = if (body.isNotBlank()) JSONObject(body) else JSONObject()
            val slotIndex = json.optInt("slot", 0).coerceIn(0, 15)
            val slots = slotListSupplier()

            if (slotIndex < slots.size) {
                val slot = slots[slotIndex]
                if (slot.portalSlot != -1) {
                    SkylanderConfig.removeSkylander(slot.portalSlot)
                }
                mainHandler.post {
                    onSlotCleared(slotIndex)
                }
                response.put("success", true)
                response.put("slot", slotIndex)
            } else {
                response.put("success", false)
                response.put("error", "Invalid slot index: $slotIndex")
            }
        } catch (e: Exception) {
            response.put("success", false)
            response.put("error", e.message ?: "Unknown error")
        }
        return response
    }

    private fun handleUploadRequest(body: String): JSONObject {
        val response = JSONObject()
        try {
            val json = JSONObject(body)
            val slotIndex = json.optInt("slot", 0).coerceIn(0, 15)
            val filename = json.optString("filename", "uploaded_figure.sky")
            val base64Data = json.optString("data", "")

            if (base64Data.isBlank()) {
                response.put("success", false)
                response.put("error", "Empty file data received")
                return response
            }

            val fileBytes = Base64.decode(base64Data, Base64.DEFAULT)
            val targetFile = File(SkylanderConfig.getSkylandersDirectory(context), SkylanderConfig.sanitizeFileName(filename))
            FileOutputStream(targetFile).use { fos ->
                fos.write(fileBytes)
            }

            val slots = slotListSupplier()
            val currentSlot = if (slotIndex < slots.size) slots[slotIndex] else null
            val portalSlotToUse = currentSlot?.portalSlot ?: slotIndex

            val loadResult = SkylanderConfig.loadSkylander(portalSlotToUse, targetFile.absolutePath)
            if (loadResult != null && loadResult.first != null) {
                val loadedPortalSlot = loadResult.first!!
                val loadedName = loadResult.second ?: filename.removeSuffix(".sky")

                mainHandler.post {
                    onSlotUpdated(slotIndex, loadedPortalSlot, loadedName)
                }

                response.put("success", true)
                response.put("slot", slotIndex)
                response.put("portalSlot", loadedPortalSlot)
                response.put("name", loadedName)
            } else {
                response.put("success", false)
                response.put("error", "Failed to parse and place uploaded figure")
            }
        } catch (e: Exception) {
            response.put("success", false)
            response.put("error", e.message ?: "Unknown error")
        }
        return response
    }
}
