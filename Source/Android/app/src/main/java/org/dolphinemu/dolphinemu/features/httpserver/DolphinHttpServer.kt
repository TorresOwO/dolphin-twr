// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.httpserver

import android.content.Context
import org.dolphinemu.dolphinemu.utils.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStream
import java.net.Inet4Address
import java.net.NetworkInterface
import java.net.ServerSocket
import java.net.Socket
import java.net.SocketException
import java.net.URLDecoder
import java.nio.charset.StandardCharsets
import java.util.concurrent.CopyOnWriteArrayList
import kotlin.concurrent.thread

/**
 * A lightweight, embedded multi-threaded HTTP server for Dolphin Android.
 * Supports modular routing via [HttpRouteModule] and static asset serving via [AssetRouteModule].
 */
class DolphinHttpServer(
    private val context: Context,
    val port: Int = 9090
) {
    private var serverSocket: ServerSocket? = null
    @Volatile
    var isRunning = false
        private set

    private val modules = CopyOnWriteArrayList<HttpRouteModule>()
    private val assetModule = AssetRouteModule(context)

    init {
        // Register built-in system routes
        registerModule(SystemRouteModule())
    }

    /**
     * Registers a new [HttpRouteModule] to handle requests for its base path.
     */
    fun registerModule(module: HttpRouteModule) {
        if (!modules.contains(module)) {
            modules.add(module)
            Log.info("DolphinHttpServer: Registered module '${module.name}' at '${module.basePath}'")
        }
    }

    /**
     * Unregisters a previously registered [HttpRouteModule].
     */
    fun unregisterModule(module: HttpRouteModule) {
        modules.remove(module)
    }

    /**
     * Starts listening for HTTP connections in the background.
     */
    fun start() {
        if (isRunning) return

        try {
            serverSocket = ServerSocket(port)
            isRunning = true
            Log.info("DolphinHttpServer: Server started on port $port")

            thread(name = "DolphinHttpServer-Acceptor", isDaemon = true) {
                while (isRunning) {
                    try {
                        val clientSocket = serverSocket?.accept() ?: break
                        thread(name = "DolphinHttpServer-Worker", isDaemon = true) {
                            handleClient(clientSocket)
                        }
                    } catch (_: SocketException) {
                        break
                    } catch (e: Exception) {
                        Log.error("DolphinHttpServer: Error accepting connection: ${e.message}")
                    }
                }
            }
        } catch (e: Exception) {
            Log.error("DolphinHttpServer: Failed to start on port $port: ${e.message}")
        }
    }

    /**
     * Stops the HTTP server and closes listening sockets.
     */
    fun stop() {
        isRunning = false
        try {
            serverSocket?.close()
        } catch (_: Exception) {
        }
        serverSocket = null
        Log.info("DolphinHttpServer: Server stopped")
    }

    private fun handleClient(socket: Socket) {
        socket.use { client ->
            try {
                val input = client.getInputStream()
                val output = client.getOutputStream()
                val reader = BufferedReader(InputStreamReader(input, StandardCharsets.UTF_8))

                // Parse request line
                val requestLine = reader.readLine() ?: return
                val parts = requestLine.split(" ")
                if (parts.size < 2) return

                val method = parts[0].uppercase()
                val rawUri = parts[1]
                val pathWithQuery = rawUri.split("#")[0]
                val path = pathWithQuery.substringBefore("?")
                val queryString = if (pathWithQuery.contains("?")) pathWithQuery.substringAfter("?") else ""

                // Parse query params
                val queryParams = parseQueryParams(queryString)

                // Read headers
                val headers = mutableMapOf<String, String>()
                var contentLength = 0
                while (true) {
                    val line = reader.readLine() ?: break
                    if (line.isEmpty()) break
                    val colonIdx = line.indexOf(':')
                    if (colonIdx > 0) {
                        val key = line.substring(0, colonIdx).trim().lowercase()
                        val value = line.substring(colonIdx + 1).trim()
                        headers[key] = value
                        if (key == "content-length") {
                            contentLength = value.toIntOrNull() ?: 0
                        }
                    }
                }

                // Read body
                var body = ""
                var rawBody = ByteArray(0)
                if (contentLength > 0) {
                    val charBuffer = CharArray(contentLength)
                    var totalRead = 0
                    while (totalRead < contentLength) {
                        val read = reader.read(charBuffer, totalRead, contentLength - totalRead)
                        if (read == -1) break
                        totalRead += read
                    }
                    body = String(charBuffer, 0, totalRead)
                    rawBody = body.toByteArray(StandardCharsets.UTF_8)
                }

                // Handle CORS pre-flight
                if (method == "OPTIONS") {
                    sendResponse(output, HttpResponse(statusCode = 204, statusText = "No Content"))
                    return
                }

                val request = HttpRequest(
                    method = method,
                    path = path,
                    rawPath = rawUri,
                    queryParams = queryParams,
                    headers = headers,
                    body = body,
                    rawBody = rawBody
                )

                // Dispatch to matching module
                var response: HttpResponse? = null
                for (module in modules) {
                    if (module.canHandle(request)) {
                        response = module.handle(request)
                        if (response != null) break
                    }
                }

                // Fallback to static assets
                if (response == null) {
                    response = assetModule.handle(request)
                }

                // Default 404 if no module or asset handled the request
                if (response == null) {
                    response = HttpResponse.notFound("404 Not Found: ${request.path}")
                }

                sendResponse(output, response)
            } catch (e: Exception) {
                Log.error("DolphinHttpServer: Error processing request: ${e.message}")
            }
        }
    }

    private fun parseQueryParams(queryString: String): Map<String, String> {
        val result = mutableMapOf<String, String>()
        if (queryString.isBlank()) return result

        for (param in queryString.split("&")) {
            val pair = param.split("=", limit = 2)
            if (pair.isNotEmpty()) {
                val key = try { URLDecoder.decode(pair[0], "UTF-8") } catch (_: Exception) { pair[0] }
                val value = if (pair.size > 1) {
                    try { URLDecoder.decode(pair[1], "UTF-8") } catch (_: Exception) { pair[1] }
                } else ""
                result[key] = value
            }
        }
        return result
    }

    private fun sendResponse(output: OutputStream, response: HttpResponse) {
        val headerBuilder = StringBuilder()
        headerBuilder.append("HTTP/1.1 ${response.statusCode} ${response.statusText}\r\n")
        headerBuilder.append("Content-Type: ${response.contentType}\r\n")
        headerBuilder.append("Content-Length: ${response.bodyBytes.size}\r\n")
        headerBuilder.append("Access-Control-Allow-Origin: *\r\n")
        headerBuilder.append("Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n")
        headerBuilder.append("Access-Control-Allow-Headers: Content-Type, Authorization\r\n")
        headerBuilder.append("Connection: close\r\n")

        for ((k, v) in response.headers) {
            headerBuilder.append("$k: $v\r\n")
        }
        headerBuilder.append("\r\n")

        output.write(headerBuilder.toString().toByteArray(StandardCharsets.UTF_8))
        if (response.bodyBytes.isNotEmpty()) {
            output.write(response.bodyBytes)
        }
        output.flush()
    }

    private inner class SystemRouteModule : HttpRouteModule {
        override val name: String = "System API"
        override val basePath: String = "/api/server"

        override fun handle(request: HttpRequest): HttpResponse? {
            if (request.path == "/api/server/modules" && request.method == "GET") {
                val json = JSONObject()
                json.put("success", true)
                json.put("running", isRunning)
                json.put("port", port)

                val modulesArray = JSONArray()
                for (module in modules) {
                    if (module === this) continue
                    val modObj = JSONObject()
                    modObj.put("name", module.name)
                    modObj.put("description", module.description)
                    modObj.put("basePath", module.basePath)
                    modObj.put("webUiPath", module.webUiPath)
                    modulesArray.put(modObj)
                }
                json.put("modules", modulesArray)
                return HttpResponse.json(json.toString())
            }
            return null
        }
    }

    companion object {
        /**
         * Resolves the primary local IPv4 address (e.g. Wi-Fi).
         */
        fun getLocalIpAddress(): String? {
            try {
                val interfaces = NetworkInterface.getNetworkInterfaces()
                while (interfaces.hasMoreElements()) {
                    val iface = interfaces.nextElement()
                    if (iface.isLoopback || !iface.isUp) continue
                    val addresses = iface.inetAddresses
                    while (addresses.hasMoreElements()) {
                        val addr = addresses.nextElement()
                        if (addr is Inet4Address && !addr.isLoopbackAddress) {
                            return addr.hostAddress
                        }
                    }
                }
            } catch (_: Exception) {
            }
            return null
        }
    }
}
