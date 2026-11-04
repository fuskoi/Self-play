const STORAGE_KEY = "private-links-extras";
const OVERRIDES_KEY = "private-links-overrides";
const DELETED_KEY = "private-links-deleted";
const AUTH_KEY = "private-links-auth";
const SORT_KEY = "private-links-sort";

const panLabel = {
  quark: "夸克网盘",
  baidu: "百度网盘",
  other: "其他链接",
};

function detectPan(url, fallback = "other") {
  const u = String(url || "").toLowerCase();
  if (/quark\.cn|pan\.quark/.test(u)) return "quark";
  if (/baidu\.com|yun\.baidu|pan\.baidu/.test(u)) return "baidu";
  return fallback === "quark" || fallback === "baidu" ? fallback : "other";
}

function normalizeLinks(item) {
  if (Array.isArray(item?.links) && item.links.length) {
    return item.links
      .map((l) => ({
        url: String(l.url || "").trim(),
        note: String(l.note || "").trim(),
      }))
      .filter((l) => l.url);
  }
  if (item?.url) {
    return [{ url: String(item.url).trim(), note: "" }];
  }
  return [];
}

function createdAtOf(item) {
  if (typeof item?.createdAt === "number") return item.createdAt;
  const m = String(item?.id || "").match(/^local-(\d+)$/);
  if (m) return Number(m[1]);
  return 0;
}

function normalizeItem(item) {
  const links = normalizeLinks(item);
  return {
    ...item,
    links,
    createdAt: createdAtOf(item),
    url: undefined,
    pan: undefined,
  };
}

async function hashPassword(text) {
  const data = new TextEncoder().encode(text);
  const buf = await crypto.subtle.digest("SHA-256", data);
  return [...new Uint8Array(buf)]
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("");
}

window.hashPassword = hashPassword;

function loadExtras() {
  try {
    return JSON.parse(localStorage.getItem(STORAGE_KEY) || "[]").map(normalizeItem);
  } catch {
    return [];
  }
}

function saveExtras(list) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(list.map(normalizeItem)));
}

function loadOverrides() {
  try {
    return JSON.parse(localStorage.getItem(OVERRIDES_KEY) || "{}");
  } catch {
    return {};
  }
}

function saveOverrides(map) {
  localStorage.setItem(OVERRIDES_KEY, JSON.stringify(map));
}

function loadDeleted() {
  try {
    return new Set(JSON.parse(localStorage.getItem(DELETED_KEY) || "[]"));
  } catch {
    return new Set();
  }
}

function saveDeleted(set) {
  localStorage.setItem(DELETED_KEY, JSON.stringify([...set]));
}

function allResources() {
  const overrides = loadOverrides();
  const deleted = loadDeleted();
  const base = (Array.isArray(window.RESOURCES) ? window.RESOURCES : [])
    .filter((r) => !deleted.has(r.id))
    .map((r) => normalizeItem({ ...r, ...(overrides[r.id] || {}) }));
  return [...base, ...loadExtras()];
}

function findById(id) {
  return allResources().find((x) => x.id === id);
}

function deleteItem(id) {
  if (isLocalId(id)) {
    saveExtras(loadExtras().filter((x) => x.id !== id));
    return;
  }
  const deleted = loadDeleted();
  deleted.add(id);
  saveDeleted(deleted);
  const map = loadOverrides();
  if (map[id]) {
    delete map[id];
    saveOverrides(map);
  }
}

function isUnlocked() {
  return sessionStorage.getItem(AUTH_KEY) === "1";
}

function setUnlocked(ok) {
  if (ok) sessionStorage.setItem(AUTH_KEY, "1");
  else sessionStorage.removeItem(AUTH_KEY);
}

const gate = document.getElementById("gate");
const app = document.getElementById("app");
const grid = document.getElementById("grid");
const empty = document.getElementById("empty");
const countEl = document.getElementById("count");
const searchInput = document.getElementById("search");
const editor = document.getElementById("editor");
const manager = document.getElementById("manager");
const managerList = document.getElementById("manager-list");
const managerEmpty = document.getElementById("manager-empty");
const editorForm = document.getElementById("editor-form");
const linksEditor = document.getElementById("links-editor");
const imagePreview = document.getElementById("image-preview");
const imagePreviewImg = document.getElementById("image-preview-img");
const editImageData = document.getElementById("edit-image-data");
const editImageUrl = document.getElementById("edit-image");
const editImageFile = document.getElementById("edit-image-file");

const PAGE_SIZE = 12;

let activeCat = "全部";
let activeSort = localStorage.getItem(SORT_KEY) === "name" ? "name" : "added";
let currentPage = 1;

const pager = document.getElementById("pager");
const pageInfo = document.getElementById("page-info");
const btnPrev = document.getElementById("btn-prev");
const btnNext = document.getElementById("btn-next");

function applyConfig() {
  const cfg = window.SITE_CONFIG || {};
  const title = cfg.title || "我的资源库";
  document.title = title;
  document.getElementById("gate-brand").textContent = title;
  document.getElementById("brand").textContent = title;
  document.getElementById("tagline").textContent =
    cfg.subtitle || "私人链接 · 仅自己可见";
}

function showApp() {
  document.body.classList.remove("is-locked");
  document.body.classList.add("is-unlocked");
  gate.hidden = true;
  app.hidden = false;
  render();
}

function showGate() {
  document.body.classList.add("is-locked");
  document.body.classList.remove("is-unlocked");
  app.hidden = true;
  gate.hidden = false;
  grid.innerHTML = "";
  countEl.textContent = "";
  if (editor.open) editor.close();
  if (manager.open) manager.close();
  document.getElementById("password").value = "";
  document.getElementById("gate-error").hidden = true;
}

function filtered() {
  const q = searchInput.value.trim().toLowerCase();
  const list = allResources().filter((item) => {
    const catOk = activeCat === "全部" || item.category === activeCat;
    if (!catOk) return false;
    if (!q) return true;
    const pans = normalizeLinks(item)
      .map((l) => panLabel[detectPan(l.url)])
      .join(" ");
    const blob = [item.title, item.note, item.category, pans]
      .filter(Boolean)
      .join(" ")
      .toLowerCase();
    return blob.includes(q);
  });

  list.sort((a, b) => {
    if (activeSort === "name") {
      return String(a.title || "").localeCompare(String(b.title || ""), "zh-CN");
    }
    // 添加顺序：先添加的在前
    return createdAtOf(a) - createdAtOf(b);
  });
  return list;
}

function syncSortButtons() {
  document.querySelectorAll(".sort-option").forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.sort === activeSort);
  });
}

function escapeHtml(str) {
  return String(str ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function escapeAttr(str) {
  return escapeHtml(str).replaceAll("'", "&#39;");
}

function coverHtml(item) {
  const src = item.image;
  if (src) {
    return `<div class="card-cover"><img src="${escapeAttr(src)}" alt="" loading="lazy" /></div>`;
  }
  const letter = (item.title || "?").trim().charAt(0).toUpperCase();
  return `<div class="card-cover"><div class="card-cover-fallback">${escapeHtml(letter)}</div></div>`;
}

function isLocalId(id) {
  return String(id || "").startsWith("local-");
}

function linksButtonsHtml(item) {
  const links = normalizeLinks(item);
  if (!links.length) {
    return `<span class="pan-tag other">暂无链接</span>`;
  }
  return links
    .map((link) => {
      const pan = detectPan(link.url);
      const label = panLabel[pan] || "其他链接";
      const title = link.note ? ` title="${escapeAttr(link.note)}"` : "";
      return `<a class="pan-tag ${escapeHtml(pan)}" href="${escapeAttr(link.url)}" target="_blank" rel="noopener noreferrer"${title}>${escapeHtml(label)}</a>`;
    })
    .join("");
}

function renderPager(total) {
  const totalPages = Math.max(1, Math.ceil(total / PAGE_SIZE));
  if (currentPage > totalPages) currentPage = totalPages;
  if (currentPage < 1) currentPage = 1;

  if (total <= PAGE_SIZE) {
    pager.hidden = true;
    return { start: 0, end: total, totalPages: 1 };
  }

  pager.hidden = false;
  pageInfo.textContent = `第 ${currentPage} / ${totalPages} 页`;
  btnPrev.disabled = currentPage <= 1;
  btnNext.disabled = currentPage >= totalPages;
  const start = (currentPage - 1) * PAGE_SIZE;
  return { start, end: start + PAGE_SIZE, totalPages };
}

function render() {
  const list = filtered();
  const { start, end } = renderPager(list.length);
  const pageList = list.slice(start, end);

  countEl.textContent =
    list.length > PAGE_SIZE
      ? `共 ${list.length} 个资源 · 本页 ${pageList.length} 个`
      : `共 ${list.length} 个资源`;
  empty.hidden = list.length > 0;
  grid.innerHTML = pageList
    .map((item, i) => {
      return `
      <article class="card" style="animation-delay:${Math.min(i, 10) * 40}ms">
        ${coverHtml(item)}
        <div class="card-body">
          <div class="card-top">
            <span class="cat-pill">${escapeHtml(item.category)}</span>
          </div>
          <h3>${escapeHtml(item.title)}</h3>
          ${item.note ? `<p class="note">备注：${escapeHtml(item.note)}</p>` : ""}
          <div class="card-actions">
            ${linksButtonsHtml(item)}
          </div>
        </div>
      </article>`;
    })
    .join("");

  if (manager.open) renderManager();
}

function renderManager() {
  const list = allResources();
  managerEmpty.hidden = list.length > 0;
  managerList.innerHTML = list
    .map((item) => {
      const links = normalizeLinks(item);
      const panText = links.length
        ? links.map((l) => panLabel[detectPan(l.url)]).join(" / ")
        : "无链接";
      const thumb = item.image
        ? `<img class="manager-thumb" src="${escapeAttr(item.image)}" alt="" />`
        : `<div class="manager-thumb fallback">${escapeHtml((item.title || "?").charAt(0))}</div>`;
      return `
      <div class="manager-row">
        ${thumb}
        <div class="manager-meta">
          <strong>${escapeHtml(item.title)}</strong>
          <span>${escapeHtml(item.category)} · ${escapeHtml(panText)}</span>
        </div>
        <div class="manager-ops">
          <button type="button" data-edit="${escapeAttr(item.id)}">修改</button>
          <button type="button" class="danger" data-del="${escapeAttr(item.id)}">删除</button>
        </div>
      </div>`;
    })
    .join("");
}

function openManager() {
  renderManager();
  manager.showModal();
}

function showPreview(src) {
  if (!src) {
    imagePreview.hidden = true;
    imagePreviewImg.removeAttribute("src");
    return;
  }
  imagePreviewImg.src = src;
  imagePreview.hidden = false;
}

function clearImageFields() {
  editImageData.value = "";
  editImageUrl.value = "";
  editImageFile.value = "";
  showPreview("");
}

function setImageFields(image) {
  clearImageFields();
  if (!image) return;
  if (image.startsWith("data:")) {
    editImageData.value = image;
    showPreview(image);
  } else {
    editImageUrl.value = image;
    showPreview(image);
  }
}

function compressImage(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error("读取图片失败"));
    reader.onload = () => {
      const img = new Image();
      img.onerror = () => reject(new Error("图片无法解析"));
      img.onload = () => {
        const maxW = 800;
        const scale = Math.min(1, maxW / img.width);
        const canvas = document.createElement("canvas");
        canvas.width = Math.round(img.width * scale);
        canvas.height = Math.round(img.height * scale);
        const ctx = canvas.getContext("2d");
        ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
        resolve(canvas.toDataURL("image/jpeg", 0.72));
      };
      img.src = reader.result;
    };
    reader.readAsDataURL(file);
  });
}

function updateLinkRowLabel(row) {
  const input = row.querySelector(".link-url");
  const label = row.querySelector(".link-pan-label");
  const pan = detectPan(input.value);
  label.textContent = panLabel[pan];
  label.className = "link-pan-label " + pan;
}

function createLinkRow(link = { url: "", note: "" }) {
  const row = document.createElement("div");
  row.className = "link-row";
  const pan = detectPan(link.url);
  row.innerHTML = `
    <div class="link-row-top">
      <strong class="link-pan-label ${pan}">${panLabel[pan]}</strong>
      <button type="button" class="btn-remove-link">删除</button>
    </div>
    <input class="link-url" type="url" required placeholder="https:// 网盘分享链接" value="${escapeAttr(link.url || "")}" />
    <input class="link-note" type="text" maxlength="40" placeholder="这条链接的提取码（可选）" value="${escapeAttr(link.note || "")}" />
  `;
  row.querySelector(".link-url").addEventListener("input", () => updateLinkRowLabel(row));
  row.querySelector(".btn-remove-link").addEventListener("click", () => {
    const rows = linksEditor.querySelectorAll(".link-row");
    if (rows.length <= 1) {
      row.querySelector(".link-url").value = "";
      row.querySelector(".link-note").value = "";
      updateLinkRowLabel(row);
      return;
    }
    row.remove();
    refreshLinkRemoveState();
  });
  return row;
}

function refreshLinkRemoveState() {
  const rows = [...linksEditor.querySelectorAll(".link-row")];
  rows.forEach((row) => {
    row.querySelector(".btn-remove-link").textContent =
      rows.length <= 1 ? "清空" : "删除";
  });
}

function setLinkRows(links) {
  linksEditor.innerHTML = "";
  const list = links?.length ? links : [{ url: "", note: "" }];
  list.forEach((link) => linksEditor.appendChild(createLinkRow(link)));
  refreshLinkRemoveState();
}

function readLinkRows() {
  return [...linksEditor.querySelectorAll(".link-row")]
    .map((row) => ({
      url: row.querySelector(".link-url").value.trim(),
      note: row.querySelector(".link-note").value.trim(),
    }))
    .filter((l) => l.url);
}

function setCategory(value) {
  const cat = value || "办公";
  document.getElementById("edit-category").value = cat;
  document.querySelectorAll(".cat-option").forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.value === cat);
  });
}

function openEditor(item) {
  editorForm.reset();
  clearImageFields();

  if (item) {
    document.getElementById("editor-title").textContent = "修改资源";
    document.getElementById("edit-id").value = item.id;
    document.getElementById("edit-title").value = item.title || "";
    document.getElementById("edit-note").value = item.note || "";
    setImageFields(item.image || "");
    setLinkRows(normalizeLinks(item));
    setCategory(item.category || "办公");
  } else {
    document.getElementById("editor-title").textContent = "添加资源";
    document.getElementById("edit-id").value = "";
    setLinkRows([{ url: "", note: "" }]);
    setCategory("办公");
  }

  editor.showModal();
}

function saveItem(payload) {
  const id = payload.id;

  if (id && isLocalId(id)) {
    const list = loadExtras();
    const idx = list.findIndex((x) => x.id === id);
    if (idx >= 0) {
      list[idx] = normalizeItem({
        ...list[idx],
        ...payload,
        createdAt: list[idx].createdAt || createdAtOf(list[idx]),
      });
      saveExtras(list);
      return;
    }
  }

  if (id && !isLocalId(id)) {
    const map = loadOverrides();
    map[id] = {
      title: payload.title,
      image: payload.image,
      links: payload.links,
      category: payload.category,
      note: payload.note,
      createdAt: payload.createdAt || createdAtOf({ id }),
    };
    saveOverrides(map);
    return;
  }

  const now = Date.now();
  saveExtras([
    ...loadExtras(),
    normalizeItem({
      ...payload,
      id: "local-" + now,
      createdAt: now,
    }),
  ]);
}

document.getElementById("gate-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const input = document.getElementById("password").value;
  const err = document.getElementById("gate-error");
  const expected = window.SITE_CONFIG?.passwordHash || "";
  const actual = await hashPassword(input);
  if (actual === expected) {
    setUnlocked(true);
    err.hidden = true;
    showApp();
  } else {
    err.hidden = false;
  }
});

document.querySelectorAll(".tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach((t) => t.classList.remove("active"));
    tab.classList.add("active");
    activeCat = tab.dataset.cat;
    currentPage = 1;
    render();
  });
});

searchInput.addEventListener("input", () => {
  currentPage = 1;
  render();
});

document.querySelector(".sort-picker").addEventListener("click", (e) => {
  const btn = e.target.closest(".sort-option");
  if (!btn) return;
  activeSort = btn.dataset.sort === "name" ? "name" : "added";
  localStorage.setItem(SORT_KEY, activeSort);
  syncSortButtons();
  currentPage = 1;
  render();
});

btnPrev.addEventListener("click", () => {
  if (currentPage > 1) {
    currentPage -= 1;
    render();
    window.scrollTo({ top: 0, behavior: "smooth" });
  }
});

btnNext.addEventListener("click", () => {
  currentPage += 1;
  render();
  window.scrollTo({ top: 0, behavior: "smooth" });
});

managerList.addEventListener("click", (e) => {
  const editBtn = e.target.closest("[data-edit]");
  if (editBtn) {
    const item = findById(editBtn.getAttribute("data-edit"));
    if (item) openEditor(item);
    return;
  }

  const delBtn = e.target.closest("[data-del]");
  if (delBtn) {
    const id = delBtn.getAttribute("data-del");
    if (confirm("确定删除这个资源？")) {
      deleteItem(id);
      render();
    }
  }
});

document.getElementById("btn-lock").addEventListener("click", () => {
  setUnlocked(false);
  showGate();
});

document.getElementById("btn-add").addEventListener("click", () => {
  openEditor(null);
});

document.getElementById("btn-manage").addEventListener("click", () => {
  openManager();
});

document.getElementById("btn-manager-close").addEventListener("click", () => {
  manager.close();
});

function buildBackup() {
  return {
    version: 1,
    app: "private-links",
    exportedAt: new Date().toISOString(),
    extras: loadExtras(),
    overrides: loadOverrides(),
    deleted: [...loadDeleted()],
  };
}

function contentKey(item) {
  const links = normalizeLinks(item)
    .map((l) => l.url)
    .sort()
    .join("|");
  return `${String(item.title || "").trim()}::${links}`;
}

function parseBackup(data) {
  if (!data || data.app !== "private-links") {
    throw new Error("不是本站的备份文件");
  }
  return {
    extras: Array.isArray(data.extras) ? data.extras.map(normalizeItem) : [],
    overrides:
      data.overrides && typeof data.overrides === "object" ? data.overrides : {},
    deleted: Array.isArray(data.deleted) ? data.deleted : [],
  };
}

function applyBackupReplace(data) {
  const { extras, overrides, deleted } = parseBackup(data);
  saveExtras(extras);
  saveOverrides(overrides);
  saveDeleted(new Set(deleted));
  return extras.length;
}

function applyBackupMerge(data) {
  const { extras: incoming, overrides: incomingOverrides } = parseBackup(data);
  const current = loadExtras();
  const existingIds = new Set(allResources().map((r) => r.id));
  const existingContent = new Set(allResources().map(contentKey));

  let added = 0;
  for (const raw of incoming) {
    const item = normalizeItem(raw);
    if (existingIds.has(item.id)) continue;
    if (existingContent.has(contentKey(item))) continue;

    // 避免 id 碰巧冲突：没有同内容时若 id 已被占用，换新 id
    let id = item.id;
    if (!id || existingIds.has(id)) {
      id = "local-" + Date.now() + "-" + added;
    }
    const next = normalizeItem({
      ...item,
      id,
      createdAt: item.createdAt || Date.now() + added,
    });
    current.push(next);
    existingIds.add(next.id);
    existingContent.add(contentKey(next));
    added += 1;
  }
  saveExtras(current);

  const map = loadOverrides();
  for (const [id, val] of Object.entries(incomingOverrides)) {
    if (map[id]) continue;
    map[id] = val;
  }
  saveOverrides(map);

  return added;
}

function exportBackup() {
  const blob = new Blob([JSON.stringify(buildBackup(), null, 2)], {
    type: "application/json",
  });
  const stamp = new Date().toISOString().slice(0, 10);
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = `资源库备份-${stamp}.json`;
  a.click();
  URL.revokeObjectURL(a.href);
}

const importFile = document.getElementById("import-file");
let importMode = "merge";

document.getElementById("btn-export").addEventListener("click", () => {
  exportBackup();
});

document.getElementById("btn-import").addEventListener("click", () => {
  importMode = "merge";
  importFile.value = "";
  importFile.click();
});

document.getElementById("btn-import-replace").addEventListener("click", () => {
  importMode = "replace";
  importFile.value = "";
  importFile.click();
});

importFile.addEventListener("change", async () => {
  const file = importFile.files?.[0];
  if (!file) return;
  try {
    const text = await file.text();
    const data = JSON.parse(text);
    if (importMode === "replace") {
      if (
        !confirm("覆盖导入会替换当前浏览器里的全部卡片数据，确定继续？")
      ) {
        return;
      }
      const n = applyBackupReplace(data);
      render();
      alert(`覆盖导入完成，当前备份内卡片 ${n} 个`);
      return;
    }

    if (
      !confirm("合并导入：只添加目前没有的卡片，已有的不会改动。确定继续？")
    ) {
      return;
    }
    const added = applyBackupMerge(data);
    render();
    alert(added > 0 ? `合并完成，新增 ${added} 个卡片` : "没有可新增的卡片（都已存在）");
  } catch (err) {
    alert(err?.message || "导入失败，请确认是本站导出的 JSON 备份");
  }
});

document.getElementById("btn-cancel").addEventListener("click", () => {
  editor.close();
});

document.getElementById("btn-add-link").addEventListener("click", () => {
  linksEditor.appendChild(createLinkRow());
  refreshLinkRemoveState();
});

document.querySelector(".cat-picker").addEventListener("click", (e) => {
  const btn = e.target.closest(".cat-option");
  if (!btn) return;
  setCategory(btn.dataset.value);
});

document.getElementById("btn-clear-image").addEventListener("click", () => {
  clearImageFields();
});

editImageUrl.addEventListener("input", () => {
  const url = editImageUrl.value.trim();
  if (url) {
    editImageData.value = "";
    editImageFile.value = "";
    showPreview(url);
  } else if (!editImageData.value) {
    showPreview("");
  }
});

editImageFile.addEventListener("change", async () => {
  const file = editImageFile.files?.[0];
  if (!file) return;
  try {
    const dataUrl = await compressImage(file);
    editImageData.value = dataUrl;
    editImageUrl.value = "";
    showPreview(dataUrl);
  } catch {
    alert("图片处理失败，请换一张试试");
  }
});

editorForm.addEventListener("submit", (e) => {
  e.preventDefault();
  const links = readLinkRows();
  if (!links.length) {
    alert("请至少填写一条网盘链接");
    return;
  }
  const image = editImageData.value.trim() || editImageUrl.value.trim();
  saveItem({
    id: document.getElementById("edit-id").value.trim(),
    title: document.getElementById("edit-title").value.trim(),
    image,
    links,
    category: document.getElementById("edit-category").value,
    note: document.getElementById("edit-note").value.trim(),
  });
  editor.close();
  render();
});

applyConfig();
syncSortButtons();
document.body.classList.add("is-locked");
if (isUnlocked()) showApp();
else showGate();
