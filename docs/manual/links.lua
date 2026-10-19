-- Rewrites chapter links per output and fails on broken links, missing images and duplicate heading IDs.
-- Metadata: manual-format is "html" (one page per chapter) or "joined" (one document for the Markdown and the PDF).

local path = pandoc.path
local fmt, errors, chapters, byfile = "joined", {}, {}, {}

local function fail(msg) errors[#errors + 1] = msg end

-- Auto ID of a heading as it would be alone, before Pandoc adds "-1" to a repeat.
local function auto_id(h)
  local doc = pandoc.read(pandoc.write(pandoc.Pandoc({ pandoc.Header(1, h.content) }), "markdown"), "markdown")
  return doc.blocks[1].identifier
end

local function scan_chapters(dir)
  local files = {}
  for _, f in ipairs(pandoc.system.list_directory(dir)) do
    if f:match("^%d%d%-.+%.md$") then files[#files + 1] = f end
  end
  table.sort(files)
  local seen = {}
  for _, f in ipairs(files) do
    local fh = assert(io.open(path.join({ dir, f }), "rb"))
    local doc = pandoc.read(fh:read("a"), "markdown-auto_identifiers")
    fh:close()
    local ch = { file = f, ids = {}, page = f == "00-index.md" and "index.html" or f:gsub("%.md$", ".html") }
    for _, b in ipairs(doc.blocks) do
      if b.t == "Header" then
        local id = b.identifier ~= "" and b.identifier or auto_id(b)
        if seen[id] then
          fail(("duplicate heading ID #%s in %s (first in %s)"):format(id, f, seen[id]))
        end
        seen[id] = f
        ch.ids[id] = true
        if not ch.first then ch.first, ch.title = id, pandoc.utils.stringify(b.content) end
      end
    end
    chapters[#chapters + 1] = ch
    byfile[f] = ch
  end
end

local function current_file()
  return path.filename(PANDOC_STATE.input_files[1])
end

local function fix_link(link)
  local target = link.target
  if target:match("^%a[%w+.-]*:") then return nil end
  local file, anchor = target:match("^([^#]*)#?(.*)$")
  local here = fmt == "html" and byfile[current_file()] or nil
  local ch = file == "" and here or byfile[file]
  if file == "" and not here then
    -- Joined output: a bare anchor can point into any chapter.
    for _, c in ipairs(chapters) do if c.ids[anchor] then ch = c end end
  end
  if not ch then
    fail(("link to missing chapter: %s"):format(target))
    return nil
  end
  if anchor ~= "" and not ch.ids[anchor] then
    fail(("link to missing anchor: %s"):format(target))
    return nil
  end
  if fmt == "html" then
    link.target = (ch == here and "" or ch.page) .. (anchor ~= "" and "#" .. anchor or "")
  else
    link.target = "#" .. (anchor ~= "" and anchor or ch.first)
  end
  return link
end

local function check_image(img)
  local src = img.src
  if src:match("^%a[%w+.-]*:") then return nil end
  local fh = io.open(src, "rb")
  if fh then fh:close() else fail(("missing image: %s"):format(src)) end
end

return {
  {
    Meta = function(meta)
      fmt = pandoc.utils.stringify(meta["manual-format"] or "joined")
      scan_chapters(path.directory(PANDOC_STATE.input_files[1]))
      if fmt == "html" then
        local here = byfile[current_file()]
        local items = {}
        for _, c in ipairs(chapters) do
          items[#items + 1] = { href = c.page, title = c.title, current = c == here }
        end
        meta.sidebar = items
        meta.pagetitle = here and here.title or nil
      end
      meta["manual-format"] = nil
      return meta
    end,
  },
  {
    -- A "contents" div becomes the list of chapters, so a new chapter needs no edit to the start page.
    Div = function(div)
      if not div.classes:includes("contents") then return nil end
      local items = {}
      for i = 2, #chapters do
        local c = chapters[i]
        items[#items + 1] = pandoc.Plain({ pandoc.Link(c.title, c.file) })
      end
      div.content = { pandoc.BulletList(items) }
      return div
    end,
  },
  {
    Link = fix_link,
    Image = check_image,
  },
  {
    Pandoc = function(doc)
      if #errors > 0 then
        error("manual: " .. table.concat(errors, "\nmanual: "), 0)
      end
    end,
  },
}
