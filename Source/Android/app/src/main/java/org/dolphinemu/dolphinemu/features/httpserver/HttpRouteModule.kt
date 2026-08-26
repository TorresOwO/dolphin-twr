// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.httpserver

/**
 * An interface implemented by any feature in Dolphin wanting to expose
 * HTTP API endpoints or Web UI routes through [DolphinHttpServer].
 */
interface HttpRouteModule {
    /** Human-readable name of the module (e.g. "Skylanders Portal"). */
    val name: String

    /** Brief description of what this module provides. */
    val description: String
        get() = ""

    /** The URL path prefix handled by this module (e.g. "/api/skylanders"). */
    val basePath: String

    /** The optional Web UI landing URL for this module (e.g. "/skylanders/"). */
    val webUiPath: String?
        get() = null

    /**
     * Checks if this module should handle the incoming [request].
     */
    fun canHandle(request: HttpRequest): Boolean {
        return request.path == basePath || request.path.startsWith("$basePath/")
    }

    /**
     * Handles the [request] and produces an [HttpResponse], or null if unhandled.
     */
    fun handle(request: HttpRequest): HttpResponse?
}
