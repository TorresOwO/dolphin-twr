// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

namespace Core
{

constexpr std::string_view DASHBOARD_HTML = R"html(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Dolphin Web Portal</title>
  <style>
    :root {
      --bg-primary: #0f172a;
      --bg-card: #1e293b;
      --bg-card-hover: #334155;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
      --accent: #38bdf8;
      --accent-hover: #0284c7;
      --border: #334155;
      --badge-get: #22c55e;
      --badge-post: #f59e0b;
      --success: #22c55e;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background-color: var(--bg-primary);
      color: var(--text-main);
      line-height: 1.5;
      padding: 24px 16px;
    }
    .container { max-width: 800px; margin: 0 auto; }
    header {
      border-bottom: 1px solid var(--border);
      padding-bottom: 16px;
      margin-bottom: 24px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 12px;
    }
    h1 { font-size: 1.5rem; color: var(--accent); }
    .status-badge {
      font-size: 0.8rem;
      font-weight: 600;
      padding: 4px 12px;
      border-radius: 9999px;
      background: var(--bg-card);
      border: 1px solid var(--border);
    }
    .status-badge.online {
      background: rgba(34, 197, 94, 0.15);
      color: var(--success);
      border-color: rgba(34, 197, 94, 0.3);
    }
    .card {
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 20px;
      margin-bottom: 24px;
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 16px;
      margin-bottom: 16px;
    }
    .status-item { display: flex; flex-direction: column; gap: 4px; }
    .status-label { font-size: 0.75rem; text-transform: uppercase; color: var(--text-muted); font-weight: 600; }
    .status-val { font-size: 1.1rem; font-weight: 600; }
    .actions { display: flex; gap: 10px; margin-top: 8px; }
    .btn {
      background: var(--accent);
      color: var(--bg-primary);
      border: none;
      padding: 8px 16px;
      border-radius: 6px;
      font-weight: 600;
      cursor: pointer;
      font-size: 0.875rem;
      transition: background 0.15s ease;
    }
    .btn:hover { background: var(--accent-hover); }
    .btn-secondary {
      background: var(--bg-card-hover);
      color: var(--text-main);
      border: 1px solid var(--border);
    }
    h2 { font-size: 1.15rem; margin-bottom: 16px; color: var(--text-main); }
    .routes-grid { display: grid; gap: 12px; }
    .route-card {
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 14px 18px;
      transition: border-color 0.15s ease;
    }
    .route-card:hover { border-color: var(--accent); }
    .route-header { display: flex; align-items: center; gap: 12px; margin-bottom: 6px; }
    .badge {
      font-size: 0.75rem;
      font-weight: 700;
      padding: 2px 8px;
      border-radius: 4px;
      color: #0f172a;
      text-transform: uppercase;
    }
    .badge-get { background: var(--badge-get); }
    .badge-post { background: var(--badge-post); }
    .route-path {
      font-family: monospace;
      color: var(--accent);
      text-decoration: none;
      font-weight: 600;
      font-size: 0.95rem;
    }
    .route-desc { font-size: 0.85rem; color: var(--text-muted); }
    footer { text-align: center; font-size: 0.75rem; color: var(--text-muted); margin-top: 32px; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>Dolphin Emulator &bull; Web Portal</h1>
      <span id="serverBadge" class="status-badge">Connecting...</span>
    </header>

    <main>
      <section class="card">
        <div class="status-grid">
          <div class="status-item">
            <span class="status-label">Emulation State</span>
            <span id="stateVal" class="status-val">Loading...</span>
          </div>
          <div class="status-item">
            <span class="status-label">Game ID</span>
            <span id="gameIdVal" class="status-val">-</span>
          </div>
          <div class="status-item">
            <span class="status-label">Game Title</span>
            <span id="titleVal" class="status-val">-</span>
          </div>
          <div class="status-item">
            <span class="status-label">Speed / FPS</span>
            <span id="speedVal" class="status-val">-</span>
          </div>
        </div>
        <div class="actions">
          <button class="btn" onclick="callApi('/api/pause')">Pause</button>
          <button class="btn btn-secondary" onclick="callApi('/api/resume')">Resume</button>
        </div>
      </section>

      <h2>Registered Endpoints</h2>
      <div id="routesList" class="routes-grid">
        <div style="color: var(--text-muted);">Loading endpoints...</div>
      </div>
    </main>

    <footer>
      <p>Dolphin Emulator &bull; Embedded HTTP Server</p>
    </footer>
  </div>

  <script>
    async function loadRoutes() {
      try {
        const res = await fetch('/api/routes');
        const data = await res.json();
        const container = document.getElementById('routesList');
        if (data.success && data.routes) {
          container.innerHTML = data.routes.filter(r => r.path !== '/').map(r => `
            <div class="route-card">
              <div class="route-header">
                <span class="badge badge-${r.method.toLowerCase()}">${r.method}</span>
                <a class="route-path" href="${r.path}">${r.path}</a>
              </div>
              <p class="route-desc">${r.description || 'API Endpoint'}</p>
            </div>
          `).join('');
        }
      } catch (err) {
        console.error('Failed to load routes:', err);
      }
    }

    async function updateStatus() {
      const badge = document.getElementById('serverBadge');
      try {
        const res = await fetch('/api/status');
        const data = await res.json();
        badge.textContent = 'Server Online';
        badge.className = 'status-badge online';
        document.getElementById('stateVal').textContent = data.state;
        document.getElementById('gameIdVal').textContent = data.game_id || 'None';
        document.getElementById('titleVal').textContent = data.game_title || 'None';
        document.getElementById('speedVal').textContent = data.speed_percent.toFixed(0) + '% (' + data.fps.toFixed(1) + ' FPS)';
      } catch (err) {
        badge.textContent = 'Offline';
        badge.className = 'status-badge';
        document.getElementById('stateVal').textContent = 'Offline';
      }
    }

    async function callApi(url) {
      try {
        await fetch(url, { method: 'POST' });
        updateStatus();
      } catch (err) {
        console.error('API call failed:', err);
      }
    }

    loadRoutes();
    updateStatus();
    setInterval(updateStatus, 1500);
  </script>
</body>
</html>
)html";

}  // namespace Core
