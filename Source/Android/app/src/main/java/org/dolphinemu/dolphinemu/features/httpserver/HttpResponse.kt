// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.httpserver

import java.nio.charset.StandardCharsets

/**
 * Represents an HTTP response sent back to the client.
 */
data class HttpResponse(
    val statusCode: Int = 200,
    val statusText: String = "OK",
    val contentType: String = "text/plain; charset=utf-8",
    val headers: Map<String, String> = emptyMap(),
    val bodyBytes: ByteArray = ByteArray(0)
) {
    companion object {
        fun ok(body: String, contentType: String = "text/plain; charset=utf-8"): HttpResponse {
            return HttpResponse(
                statusCode = 200,
                statusText = "OK",
                contentType = contentType,
                bodyBytes = body.toByteArray(StandardCharsets.UTF_8)
            )
        }

        fun json(jsonString: String, statusCode: Int = 200): HttpResponse {
            return HttpResponse(
                statusCode = statusCode,
                statusText = if (statusCode == 200) "OK" else "Response",
                contentType = "application/json; charset=utf-8",
                bodyBytes = jsonString.toByteArray(StandardCharsets.UTF_8)
            )
        }

        fun html(htmlContent: String): HttpResponse {
            return HttpResponse(
                statusCode = 200,
                statusText = "OK",
                contentType = "text/html; charset=utf-8",
                bodyBytes = htmlContent.toByteArray(StandardCharsets.UTF_8)
            )
        }

        fun bytes(data: ByteArray, contentType: String): HttpResponse {
            return HttpResponse(
                statusCode = 200,
                statusText = "OK",
                contentType = contentType,
                bodyBytes = data
            )
        }

        fun redirect(location: String): HttpResponse {
            return HttpResponse(
                statusCode = 302,
                statusText = "Found",
                contentType = "text/plain",
                headers = mapOf("Location" to location),
                bodyBytes = "Redirecting to $location".toByteArray(StandardCharsets.UTF_8)
            )
        }

        fun notFound(message: String = "404 Not Found"): HttpResponse {
            return HttpResponse(
                statusCode = 404,
                statusText = "Not Found",
                contentType = "text/plain; charset=utf-8",
                bodyBytes = message.toByteArray(StandardCharsets.UTF_8)
            )
        }

        fun badRequest(message: String = "400 Bad Request"): HttpResponse {
            return HttpResponse(
                statusCode = 400,
                statusText = "Bad Request",
                contentType = "application/json; charset=utf-8",
                bodyBytes = "{\"success\":false,\"error\":\"$message\"}".toByteArray(StandardCharsets.UTF_8)
            )
        }

        fun serverError(message: String = "500 Internal Server Error"): HttpResponse {
            return HttpResponse(
                statusCode = 500,
                statusText = "Internal Server Error",
                contentType = "application/json; charset=utf-8",
                bodyBytes = "{\"success\":false,\"error\":\"$message\"}".toByteArray(StandardCharsets.UTF_8)
            )
        }
    }
}
