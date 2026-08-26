// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.httpserver

import android.content.Context
import java.io.IOException

/**
 * Serves static web assets packaged under Android's assets/web/ directory.
 */
class AssetRouteModule(
    private val context: Context,
    private val webAssetRoot: String = "web"
) : HttpRouteModule {

    override val name: String = "Static Assets"
    override val basePath: String = "/"

    override fun canHandle(request: HttpRequest): Boolean {
        // Handle root, web directories, and common static file extensions
        return request.method.equals("GET", ignoreCase = true)
    }

    override fun handle(request: HttpRequest): HttpResponse? {
        val rawPath = request.path.trim('/')
        val assetPath = if (rawPath.isEmpty()) {
            "$webAssetRoot/hub/index.html"
        } else if (!rawPath.contains('.')) {
            // Path without extension, treat as directory and try index.html
            "$webAssetRoot/$rawPath/index.html"
        } else {
            "$webAssetRoot/$rawPath"
        }

        return try {
            context.assets.open(assetPath).use { stream ->
                val bytes = stream.readBytes()
                val mimeType = getMimeType(assetPath)
                HttpResponse.bytes(bytes, mimeType)
            }
        } catch (_: IOException) {
            null
        }
    }

    private fun getMimeType(path: String): String {
        return when {
            path.endsWith(".html", ignoreCase = true) || path.endsWith(".htm", ignoreCase = true) -> "text/html; charset=utf-8"
            path.endsWith(".css", ignoreCase = true) -> "text/css; charset=utf-8"
            path.endsWith(".js", ignoreCase = true) -> "application/javascript; charset=utf-8"
            path.endsWith(".json", ignoreCase = true) -> "application/json; charset=utf-8"
            path.endsWith(".svg", ignoreCase = true) -> "image/svg+xml"
            path.endsWith(".png", ignoreCase = true) -> "image/png"
            path.endsWith(".jpg", ignoreCase = true) || path.endsWith(".jpeg", ignoreCase = true) -> "image/jpeg"
            path.endsWith(".gif", ignoreCase = true) -> "image/gif"
            path.endsWith(".ico", ignoreCase = true) -> "image/x-icon"
            path.endsWith(".woff2", ignoreCase = true) -> "font/woff2"
            path.endsWith(".woff", ignoreCase = true) -> "font/woff"
            path.endsWith(".ttf", ignoreCase = true) -> "font/ttf"
            else -> "application/octet-stream"
        }
    }
}
