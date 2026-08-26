// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.httpserver

/**
 * Represents an incoming HTTP request handled by [DolphinHttpServer].
 */
data class HttpRequest(
    val method: String,
    val path: String,
    val rawPath: String,
    val queryParams: Map<String, String>,
    val headers: Map<String, String>,
    val body: String,
    val rawBody: ByteArray
) {
    fun header(name: String): String? = headers[name.lowercase()]

    fun query(name: String): String? = queryParams[name]
}
