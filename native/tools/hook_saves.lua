-- Test hook of the saves comparison (oracle side; the port's is src/hook_saves.c: same steps, same lines).
-- Environment (frames): NOTUT=1 (tutorial marked done at frame 2), START=<f> with DECK=<b_key> STAKE=<n> SEED=<text>
-- CHALLENGE=<index of G.CHALLENGES>, SELECT=<f> (select the small blind; with BOSS=1 the boss, as the last ante's), PLAY=<f>, PLAY2..PLAY4 (play the first 5 cards of the
-- hand), CONT=<f> (continue the saved run as the run setup's Continue does), SUM=<n> (every n frames a summary),
-- LANGAT=<f> with GLANG=<key> (set the language as the language menu does), PROFILE=<f> with PROFILE_N (load that profile),
-- EASY=<f> (the blind needs 1 chip), NEXT=<f> (leave the shop), DUMPSAVE=<f> (print the
-- profile's save.jkr, keys sorted),
-- DELPROFILE=<f> (delete the current profile as the button does), UNLOCKS=<f>, UNLOCKS2=<f> (print the unlock/discover counts).
local frame = ...
local E = os.getenv
local function at(k) return tonumber(E(k) or '') == frame end

local function dump(v, k)
  if type(v) == 'number' or type(v) == 'boolean' then return tostring(v) end
  if type(v) == 'string' then return v end
  if type(v) ~= 'table' or v.is then return 'obj' end
  local keys, out = {}, {}
  for kk, vv in pairs(v) do
    local s = tostring(kk)
    if not s:find('colour') and s ~= 'blind' then keys[#keys + 1] = s; out[s] = vv end
  end
  table.sort(keys)
  for i, s in ipairs(keys) do keys[i] = s .. '=' .. dump(out[s]) end
  return '{' .. table.concat(keys, ',') .. '}'
end

local function cards(area)
  if not area or not area.cards then return 'nil' end
  local t = {}
  for i, c in ipairs(area.cards) do
    t[i] = (c.base.value and (c.base.value .. c.base.suit:sub(1, 1) .. '/') or '') .. c.ability.name ..
      (c.edition and '+' .. c.edition.type or '') .. (c.seal and '+' .. c.seal or '') ..
      (c.ability.eternal and '+E' or '') .. (c.ability.perishable and '+P' or '') .. (c.ability.rental and '+R' or '') ..
      (c.pinned and '+pin' or '') .. (c.debuff and '+D' or '') .. (c.facing == 'back' and '+B' or '') .. '$' .. tostring(c.sell_cost)
  end
  return #t .. ': ' .. table.concat(t, ' ')
end

local function sum()
  if G.STAGE ~= G.STAGES.RUN or not G.deck or not G.GAME then return end
  local p = 'SUM ' .. frame .. ' '
  print(p .. 'state ' .. tostring(G.STATE) .. ' back ' .. tostring(G.GAME.selected_back.name) .. ' blind ' .. tostring(G.GAME.blind and G.GAME.blind.name) ..
    ' blindchips ' .. tostring(G.GAME.blind and G.GAME.blind.chips) .. ' handlimit ' .. tostring(G.hand.config.card_limit) ..
    ' jokerlimit ' .. tostring(G.jokers.config.card_limit) .. ' conslimit ' .. tostring(G.consumeables.config.card_limit) ..
    ' tags ' .. #G.GAME.tags .. ' overlay ' .. tostring(G.OVERLAY_MENU ~= nil and G.OVERLAY_MENU ~= false) .. ' paused ' .. tostring(G.SETTINGS.paused))
  for _, k in ipairs{'deck', 'hand', 'play', 'discard', 'jokers', 'consumeables', 'shop_jokers', 'shop_vouchers', 'shop_booster'} do
    print(p .. k .. ' ' .. cards(G[k]))
  end
  local tags = {}
  for i, t in ipairs(G.GAME.tags) do
    tags[i] = t.key .. '/' .. tostring(t.ability.orbital_hand) .. '/' .. tostring(t.ability.blind_type) .. '/' .. tostring(t.tally) .. '/' .. tostring(t.HUD_tag ~= nil)
  end
  print(p .. 'tags ' .. table.concat(tags, ' '))
  local order = {}
  for k, v in pairs(G.GAME.hands) do order[#order + 1] = k end
  print(p .. 'handorder ' .. table.concat(order, ','))
  local keys = {}
  for k, v in pairs(G.GAME) do keys[#keys + 1] = tostring(k) end
  table.sort(keys)
  for _, k in ipairs(keys) do
    if k ~= 'challenge_tab' then print(p .. 'G.' .. k .. ' ' .. dump(G.GAME[k])) end
  end
end

if frame == 2 and E('NOTUT') then G.SETTINGS.tutorial_complete = true end
if at('LANGAT') and E('GLANG') then
  -- button_callbacks.lua:2408 G.FUNCS.change_lang, without the confirmation overlay
  G.SETTINGS.language = E('GLANG')
  G:set_language()
  G.E_MANAGER:clear_queue()
  G.FUNCS.wipe_on()
  G.E_MANAGER:add_event(Event({no_delete = true, blockable = true, blocking = false, func = function()
    G:delete_run()
    G:init_item_prototypes()
    G:main_menu()
    return true
  end}))
  G.FUNCS.wipe_off()
end
if at('START') then
  if E('DECK') then G.GAME.viewed_back = Back(G.P_CENTERS[E('DECK')]) end
  G.FUNCS.start_run(nil, {stake = tonumber(E('STAKE') or ''), seed = E('SEED'), challenge = E('CHALLENGE') and G.CHALLENGES[tonumber(E('CHALLENGE'))]})
end
if at('CONT') then
  -- UI_definitions.lua:5942 (G.UIDEF.run_setup) and button_callbacks.lua:1840 (the Continue of start_setup_run)
  G.SAVED_GAME = get_compressed(G.SETTINGS.profile .. '/' .. 'save.jkr')
  if G.SAVED_GAME ~= nil then G.SAVED_GAME = STR_UNPACK(G.SAVED_GAME) end
  print('SUM ' .. frame .. ' saved ' .. tostring(G.SAVED_GAME ~= nil))
  if G.SAVED_GAME ~= nil then G.FUNCS.start_run(nil, {savetext = G.SAVED_GAME}) end
end
if at('SELECT') and G.blind_select_opts then
  if E('BOSS') then  -- the boss of the last ante: beating it wins the run
    G.GAME.round_resets.ante = G.GAME.win_ante
    G.GAME.blind_on_deck = 'Boss'
    G.FUNCS.select_blind(G.blind_select_opts.boss:get_UIE_by_ID('select_blind_button'))
  else
    G.FUNCS.select_blind(G.blind_select_opts.small:get_UIE_by_ID('select_blind_button'))
  end
end
if (at('PLAY') or at('PLAY2') or at('PLAY3') or at('PLAY4')) and G.hand and G.STATE == G.STATES.SELECTING_HAND then
  for i = 1, 5 do if G.hand.cards[i] then G.hand:add_to_highlighted(G.hand.cards[i], true) end end
  G.FUNCS.play_cards_from_highlighted()
end
if at('EASY') and G.GAME.blind then G.GAME.blind.chips = 1; G.GAME.blind.chip_text = '1' end
if at('NEXT') and G.shop then G.FUNCS.toggle_shop() end
if at('UNSEED') and G.GAME then G.GAME.seeded = nil end  -- (a run with a known seed that counts for the profile's stats and unlocks)
local exitov = tonumber(E('EXITOV') or '')  -- (every 60 frames, 10 times: the unlock notices come one after another)
if exitov and frame >= exitov and frame < exitov + 600 and (frame - exitov) % 60 == 0 and G.OVERLAY_MENU then print('SUM ' .. frame .. ' overlay closed'); G.FUNCS.exit_overlay_menu() end
if at('TAGS') and G.GAME then  -- three tags that stay held: for Tag:save / Tag:load
  add_tag(Tag('tag_orbital', false, 'Small'))
  add_tag(Tag('tag_juggle'))
  add_tag(Tag('tag_investment'))
end
if at('FORCESAVE') and G.FILE_HANDLER then G.FILE_HANDLER.force = true end  -- (the queued saves are written now, not at the 30 s mark)
if at('DUMPSAVE') then
  local t = get_compressed(G.SETTINGS.profile .. '/' .. 'save.jkr')
  t = t and STR_UNPACK(t) or {}
  local keys = {}
  for k, v in pairs(t) do keys[#keys + 1] = tostring(k) end
  table.sort(keys)
  for _, k in ipairs(keys) do
    if k == 'GAME' or k == 'cardAreas' then
      local keys2 = {}
      for k2, v in pairs(t[k]) do keys2[#keys2 + 1] = tostring(k2) end
      table.sort(keys2)
      for _, k2 in ipairs(keys2) do print('SUM ' .. frame .. ' save.' .. k .. '.' .. k2 .. ' ' .. dump(t[k][k2])) end
    else
      print('SUM ' .. frame .. ' save.' .. k .. ' ' .. dump(t[k]))
    end
  end
end
if at('PROFILE') then
  -- button_callbacks.lua:1344 G.FUNCS.load_profile
  G.SETTINGS.profile = tonumber(E('PROFILE_N'))
  G.focused_profile = G.SETTINGS.profile
  G.FUNCS.load_profile(false)
end
if at('DELPROFILE') then
  G.focused_profile = G.SETTINGS.profile
  G.FUNCS.load_profile(true)
end
if at('UNLOCKS') or at('UNLOCKS2') then
  local p = 'SUM ' .. frame .. ' '
  local n = {u = 0, d = 0, a = 0}
  for k, v in pairs(G.P_CENTERS) do
    if v.unlocked then n.u = n.u + 1 end
    if v.discovered then n.d = n.d + 1 end
    if v.alerted then n.a = n.a + 1 end
  end
  print(p .. 'centers unlocked ' .. n.u .. ' discovered ' .. n.d .. ' alerted ' .. n.a .. ' profile ' .. tostring(G.SETTINGS.profile))
  local P = G.PROFILES[G.SETTINGS.profile]
  for _, k in ipairs{'career_stats', 'progress', 'high_scores', 'joker_usage', 'deck_usage', 'hand_usage', 'consumeable_usage', 'voucher_usage', 'challenge_progress', 'MEMORY', 'stake', 'all_unlocked', 'name'} do
    print(p .. 'P.' .. k .. ' ' .. (P[k] == nil and 'nil' or dump(P[k])))
  end
  local keys = {}
  for k, v in pairs(G.SETTINGS) do keys[#keys + 1] = tostring(k) end
  table.sort(keys)
  for _, k in ipairs(keys) do
    if k ~= 'WINDOW' then print(p .. 'S.' .. k .. ' ' .. dump(G.SETTINGS[k])) end
  end
end
if tonumber(E('SUM') or '') and frame % tonumber(E('SUM')) == 0 then sum() end
if E('FLAMES') and frame % tonumber(E('FLAMES')) == 0 then  -- (not a SUM line: floats of the cosmetic side)
  local F, AC = G.ARGS.chip_flames or {}, G.SETTINGS.ambient_control or {}
  print(('FLM %d intensity %s change %s fire2 %s organ %s'):format(frame, tostring(F.real_intensity), tostring(F.change),
    tostring(AC.ambientFire2 and AC.ambientFire2.vol), tostring(AC.ambientOrgan1 and AC.ambientOrgan1.vol)))
end
