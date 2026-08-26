let catalog = [];

async function init() {
  await loadCatalog();
  await loadStatus();
  setInterval(loadStatus, 2500);

  document.getElementById('btnRefresh').addEventListener('click', loadStatus);
  document.getElementById('searchInput').addEventListener('input', filterCatalog);
  document.getElementById('btnUpload').addEventListener('click', handleUpload);
}

async function loadCatalog() {
  try {
    const res = await fetch('/api/skylanders/catalog');
    catalog = await res.json();
    renderCatalog(catalog.slice(0, 15));
  } catch (err) {
    console.error('Failed to load figure catalog:', err);
  }
}

function filterCatalog() {
  const query = document.getElementById('searchInput').value.toLowerCase().trim();
  if (!query) {
    renderCatalog(catalog.slice(0, 15));
    return;
  }
  const filtered = catalog.filter(item => item.name.toLowerCase().includes(query)).slice(0, 20);
  renderCatalog(filtered);
}

function renderCatalog(items) {
  const list = document.getElementById('catalogList');
  if (items.length === 0) {
    list.innerHTML = '<div style="color: var(--text-muted); padding: 8px;">No matching figures found.</div>';
    return;
  }
  list.innerHTML = items.map(item => `
    <div class="catalog-item" onclick="placeFigure(${item.id}, ${item.variant}, '${escapeHtml(item.name)}')">
      <span class="catalog-item-name">${escapeHtml(item.name)}</span>
      <span class="btn-primary" style="padding: 2px 8px; font-size: 0.75rem;">Place</span>
    </div>
  `).join('');
}

async function loadStatus() {
  const badge = document.getElementById('portalStatusBadge');
  const grid = document.getElementById('portalSlotsGrid');
  try {
    const res = await fetch('/api/skylanders/status');
    const data = await res.json();
    if (data.success) {
      badge.textContent = 'Portal Active';
      badge.className = 'status-badge online';

      grid.innerHTML = data.slots.map(slot => `
        <div class="slot-card ${slot.occupied ? 'occupied' : ''}">
          <div class="slot-card-header">
            <span class="slot-title">Slot ${slot.slot + 1}</span>
            <span class="badge" style="${slot.occupied ? 'background: rgba(168, 85, 247, 0.2); color: #c084fc;' : 'background: rgba(148, 163, 184, 0.1); color: var(--text-muted);'}">
              ${slot.occupied ? 'Occupied' : 'Empty'}
            </span>
          </div>
          <div class="slot-name">${escapeHtml(slot.name)}</div>
          <div class="slot-actions">
            ${slot.occupied ? `
              <button class="btn-danger" onclick="removeFigure(${slot.slot})">Remove</button>
            ` : `
              <button class="btn-secondary" style="width: 100%; font-size: 0.8rem;" onclick="selectSlotForPlace(${slot.slot})">Select Slot</button>
            `}
          </div>
        </div>
      `).join('');
    }
  } catch (err) {
    badge.textContent = 'Disconnected';
    badge.className = 'status-badge offline';
  }
}

function selectSlotForPlace(slotIndex) {
  document.getElementById('slotSelect').value = slotIndex;
  document.getElementById('searchInput').focus();
  showToast(`Slot ${slotIndex + 1} selected for placement`);
}

async function placeFigure(id, variant, name) {
  const slotIndex = parseInt(document.getElementById('slotSelect').value, 10);
  showToast(`Placing ${name} on Slot ${slotIndex + 1}...`);
  try {
    const res = await fetch('/api/skylanders/load', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ slot: slotIndex, id: id, variant: variant, name: name })
    });
    const result = await res.json();
    if (result.success) {
      showToast(`Placed ${result.name} on Slot ${slotIndex + 1}`);
      await loadStatus();
    } else {
      showToast(`Error: ${result.error || 'Failed to place figure'}`);
    }
  } catch (err) {
    showToast(`Network error placing figure: ${err.message}`);
  }
}

async function removeFigure(slotIndex) {
  showToast(`Removing figure from Slot ${slotIndex + 1}...`);
  try {
    const res = await fetch('/api/skylanders/remove', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ slot: slotIndex })
    });
    const result = await res.json();
    if (result.success) {
      showToast(`Slot ${slotIndex + 1} cleared`);
      await loadStatus();
    } else {
      showToast(`Error removing figure: ${result.error}`);
    }
  } catch (err) {
    showToast(`Network error: ${err.message}`);
  }
}

async function handleUpload() {
  const fileInput = document.getElementById('skyFileInput');
  if (!fileInput.files || fileInput.files.length === 0) {
    showToast('Please choose a .sky file first');
    return;
  }
  const file = fileInput.files[0];
  const slotIndex = parseInt(document.getElementById('slotSelect').value, 10);

  showToast(`Uploading ${file.name}...`);
  try {
    const base64Data = await readFileAsBase64(file);
    const res = await fetch('/api/skylanders/upload', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        slot: slotIndex,
        filename: file.name,
        data: base64Data
      })
    });
    const result = await res.json();
    if (result.success) {
      showToast(`Successfully loaded ${result.name} on Slot ${slotIndex + 1}`);
      fileInput.value = '';
      await loadStatus();
    } else {
      showToast(`Upload failed: ${result.error}`);
    }
  } catch (err) {
    showToast(`Upload error: ${err.message}`);
  }
}

function readFileAsBase64(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => {
      const result = reader.result;
      const base64 = result.substring(result.indexOf(',') + 1);
      resolve(base64);
    };
    reader.onerror = error => reject(error);
    reader.readAsDataURL(file);
  });
}

function showToast(message) {
  const toast = document.getElementById('toast');
  toast.textContent = message;
  toast.className = 'toast show';
  setTimeout(() => {
    toast.className = 'toast';
  }, 3000);
}

function escapeHtml(str) {
  if (!str) return '';
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#039;');
}

document.addEventListener('DOMContentLoaded', init);
