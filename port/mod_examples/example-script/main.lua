-- Hidden Stats HUD: an example mod script (see port/MODDING.md, "Scripts").

-- lain.sym gives a game variable's address; lain.read16 reads it.
local stats = {
  { "Heart-flutter", lain.sym("g_stat_tokimeki") },
  { "Dejection", lain.sym("g_stat_gakkuri") },
  { "Harumage", lain.sym("g_stat_harumage") },
  { "Screensavers", lain.sym("g_screensaver_count") },
}

-- Saved between sessions in the data folder (lain.save / lain.load).
local shown = lain.load("shown") ~= "0"

-- Counting nodes opened this session: a hook on the function the game calls.
local opened = 0
lain.hook("media_play", function(next, media, skipped, params)
  opened = opened + 1
  return next(media, skipped, params)
end)

-- Drawn over the game every frame.
lain.on("draw", function()
  if not shown then return end
  lain.ui.draw_rect(0.01, 0.01, 0.30, 0.02 + 0.045 * (#stats + 1), 0, 0, 0, 0.55)
  for i, s in ipairs(stats) do
    lain.ui.draw_text(0.02, 0.015 + 0.045 * (i - 1), string.format("%-13s %3d", s[1], lain.read16s(s[2])), 0.8, 1, 0.8, 9)
  end
  lain.ui.draw_text(0.02, 0.015 + 0.045 * #stats, string.format("Nodes opened  %3d", opened), 0.8, 0.9, 1, 9)
end)

-- A switch in the F1 menu's Mods tab.
lain.on("menu", function()
  local now = lain.ui.checkbox("Show the stats", shown)
  if now ~= shown then
    shown = now
    lain.save("shown", shown and "1" or "0")
  end
end)
