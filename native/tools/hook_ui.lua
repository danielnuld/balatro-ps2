-- Test hook of the ui comparison (oracle side; src/hook_ui.c does the same steps in the port).
-- STEPS="1250:start:SEED 1500:blind 1700:ui:HUD ..." in the environment: at each frame, one command.
-- Every time the pad's focused node changes, an "F" line is printed (no command needed).
-- Commands: start:<seed>[:<stake>[:<deck key>]] | blind | win | lose | wingame | cash | money:<n> | give:<center key> |
-- ed:<area>:<i>:<foil|holo|polychrome|negative> | stick:<area>:<i>:<eternal|perishable|rental> | debuff:<area>:<i> |
-- hl:<area>:<i>[:<j>...] | play | discard | hover:<area>:<i> | unhover:<area>:<i> | chips:<n> (G.GAME.chips) |
-- handlvl:<hand with _ for spaces>:<n> | ui:<field of G that is a UIBox> (its tree as "U" lines) | pop (the popup of the focused node) |
-- pop:<area>:<i> (the popup of a card) | popall:<set>[:<variant>] | unlockall | order:<area> | tagpop | nf | discover:<key> | unlock:<key> | chal:<i> | tabs | cyc:<k>:<n> | fa:<name>:<n> | settings (G.SETTINGS as sorted "D" lines) | f:<name of a G.FUNCS> (called with an empty element)
-- FIXSEED=<seed>: every unseeded run gets this seed (the game takes it from the cursor's position and time: floats in the port).
local frame = ...
if frame == 1 then  -- math.log(x, base) as LuaJIT (the PC game) computes it: Lua 5.1 ignores the base (the development log, saves)
  local l = math.log
  -- lib_math.c: log2(x) * (1/log2(b)), which is exact at the powers of ten the game floors (1e1..1e15); Lua 5.1 has no log2 and
  -- log(x)/log(b) falls just under them, so a result within 1e-13 of an integer is that integer
  math.log = function(x, b)
    if not b then return l(x) end
    local r = l(x) / l(b)
    local n = math.floor(r + 0.5)
    return math.abs(r - n) < 1e-13 and n or r
  end
end
if frame == 1 and os.getenv("FIXSEED") then
  local s = os.getenv("FIXSEED")
  generate_starting_seed = function() return s end
end
if not STEPS_UI then
  STEPS_UI = {}
  for at, cmd in (os.getenv("STEPS") or ""):gmatch("(%d+):(%S+)") do
    STEPS_UI[tonumber(at)] = STEPS_UI[tonumber(at)] or {}
    table.insert(STEPS_UI[tonumber(at)], cmd)
  end
end

local AREAS = {'hand', 'jokers', 'consumeables', 'shop_jokers', 'shop_booster', 'shop_vouchers', 'pack_cards', 'play', 'deck', 'discard'}

local function f2(x)
  local s = ('%.2f'):format(x)
  return s == '-0.00' and '0.00' or s
end

local function card_desc(c)
  if c.facing == 'back' then return tostring(c.config.center and c.config.center.key) .. '/back' end  -- (the menus' piles are picked with math.random)
  return tostring(c.config.center and c.config.center.key) .. '/' .. tostring(c.base and c.base.suit) .. '/' .. tostring(c.base and c.base.value) ..
    (c.debuff and '/debuff' or '') .. '/' .. tostring(c.facing)
end

local function first_text(e)
  if e.config and e.config.text then return tostring(e.config.text) end
  if e.config and e.config.object and e.config.object:is(DynaText) then local d = e.config.object.string; return 'dyna:' .. (type(d) == 'string' and d or 'nil') end
  for _, v in ipairs(e.children) do
    local t = first_text(v)
    if t then return t end
  end
end

local function desc(t)
  if not t then return 'nil' end
  if t:is(Card) then
    local name = '?'
    for _, n in ipairs(AREAS) do if G[n] and G[n] == t.area then name = n end end
    return 'card ' .. name .. ' ' .. tostring(t.rank) .. ' ' .. card_desc(t)
  elseif t:is(UIElement) then
    return 'ui b=' .. tostring(t.config.button) .. ' id=' .. tostring(t.config.id) .. ' @' .. f2(t.T.x) .. ',' .. f2(t.T.y) .. ' ' .. (first_text(t) or 'nil')
  end
  return 'other @' .. f2(t.T.x) .. ',' .. f2(t.T.y)
end

local dump_box

local function dump_el(p, path, e)
  local c = e.config
  local s = p .. path .. ' n=' .. tostring(e.UIT) .. ' ' .. f2(e.T.x) .. ' ' .. f2(e.T.y) .. ' ' .. f2(e.T.w) .. ' ' .. f2(e.T.h)
  if c.text then s = s .. ' text=' .. tostring(c.text) end
  if c.scale then s = s .. ' scale=' .. f2(c.scale) end
  if c.id then s = s .. ' id=' .. tostring(c.id) end
  if c.button then s = s .. ' button=' .. tostring(c.button) end
  if c.func then s = s .. ' func=' .. tostring(c.func) end
  if c.chosen then s = s .. ' chosen=' .. tostring(c.chosen) end
  if c.colour and (e.UIT == G.UIT.T or c.button) then s = s .. ' col=' .. f2(c.colour[1]) .. ',' .. f2(c.colour[2]) .. ',' .. f2(c.colour[3]) .. ',' .. f2(c.colour[4]) end
  local o = c.object
  if o then
    if o:is(UIBox) then s = s .. ' obj=UIBox'
    elseif o:is(DynaText) then s = s .. ' dyna=' .. (type(o.string) == 'string' and o.string or 'nil') .. ' dscale=' .. f2(o.scale)
    elseif o:is(Card) then s = s .. ' card=' .. card_desc(o)
    elseif o:is(CardArea) then
      s = s .. ' area=' .. #o.cards
      for _, k in ipairs(o.cards) do s = s .. ' ' .. card_desc(k) end
    else s = s .. ' obj=other' end
  end
  print(s)
  if o and o:is(UIBox) then dump_box(p, path .. '.o', o) end
  for i, v in ipairs(e.children) do dump_el(p, path .. '.' .. i, v) end
  for _, k in ipairs({'button_pip', 'alert', 'h_popup'}) do  -- the boxes hung by name: the pad's button prompt, the "!" of a new item, a tooltip
    if e.children[k] then dump_box(p, path .. '.' .. k, e.children[k]) end
  end
end

dump_box = function(p, path, box)
  if type(box) ~= 'table' or not box.UIRoot then print(p .. path .. ' none'); return end
  dump_el(p, path, box.UIRoot)
end

local function dump(pre, v, depth)
  local t = type(v)
  if t == 'number' then print(pre .. '=' .. ('%.10g'):format(v))
  elseif t == 'string' or t == 'boolean' then print(pre .. '=' .. tostring(v))
  elseif t == 'table' then
    if getmetatable(v) then print(pre .. '=<obj>'); return end
    if depth > 8 then return end
    for k, x in pairs(v) do
      if not (type(k) == 'string' and k:find('colour')) then dump(pre .. '.' .. tostring(k), x, depth + 1) end
    end
  end
end

local function collect(e, pred, out)
  if pred(e) then out[#out + 1] = e end
  local o = e.config.object
  if o and o:is(UIBox) then collect(o.UIRoot, pred, out) end
  for _, v in ipairs(e.children) do collect(v, pred, out) end
  return out
end

local function at(area, i) i = tonumber(i); return G[area].cards[i == 0 and #G[area].cards or i] end

local function split(s)
  local r = {}
  for w in s:gmatch("[^:]+") do r[#r + 1] = w end
  return r
end

for _, cmd in ipairs(STEPS_UI[frame] or {}) do
  local a = split(cmd)
  local c = a[1]
  print('STEP ' .. frame .. ' ' .. cmd .. ' state ' .. tostring(G.STATE))
  if c == 'start' then
    G.SETTINGS.tutorial_complete = true
    G.SETTINGS.tutorial_progress = nil
    if a[4] then G.GAME.viewed_back = Back(G.P_CENTERS[a[4]]) end
    G.FUNCS.start_run(nil, {stake = tonumber(a[3]) or 1, seed = a[2]})
  elseif c == 'blind' then
    G.FUNCS.select_blind(G.blind_select_opts[string.lower(G.GAME.blind_on_deck)]:get_UIE_by_ID('select_blind_button'))
  elseif c == 'win' then
    G.GAME.chips = G.GAME.blind.chips
    G.STATE = G.STATES.HAND_PLAYED
    G.STATE_COMPLETE = false
  elseif c == 'lose' then
    G.STATE = G.STATES.GAME_OVER
    G.STATE_COMPLETE = false
  elseif c == 'wingame' then
    win_game()
  elseif c == 'cash' then
    G.FUNCS.cash_out({config = {}})
  elseif c == 'money' then
    ease_dollars(tonumber(a[2]), true)
  elseif c == 'chips' then
    G.GAME.chips = tonumber(a[2])
  elseif c == 'handlvl' then
    level_up_hand(nil, a[2]:gsub('_', ' '), true, tonumber(a[3]))
  elseif c == 'give' then
    local center = G.P_CENTERS[a[2]]
    local area = center.set == 'Joker' and G.jokers or G.consumeables
    local card = create_card(center.set, area, nil, nil, nil, nil, a[2])
    card:add_to_deck()
    area:emplace(card)
  elseif c == 'ed' then
    at(a[2], a[3]):set_edition({[a[4]] = true}, true, true)
  elseif c == 'stick' then
    local card = at(a[2], a[3])
    if a[4] == 'eternal' then card:set_eternal(true) elseif a[4] == 'perishable' then card:set_perishable(true) else card:set_rental(true) end
  elseif c == 'debuff' then
    at(a[2], a[3]):set_debuff(true)
  elseif c == 'hl' then
    for i = 3, #a do G[a[2]]:add_to_highlighted(at(a[2], a[i]), true) end
  elseif c == 'play' then
    G.FUNCS.play_cards_from_highlighted()
  elseif c == 'discard' then
    G.FUNCS.discard_cards_from_highlighted()
  elseif c == 'hover' then
    at(a[2], a[3]):hover()
  elseif c == 'unhover' then
    at(a[2], a[3]):stop_hover()
  elseif c == 'ui' then
    dump_box('U ' .. frame .. ' ' .. a[2] .. ' ', 'r', G[a[2]])
  elseif c == 'areaui' then  -- areaui:<area>:<child>: a box a card area hangs from itself (area_uibox, peek_deck, view_deck)
    dump_box('U ' .. frame .. ' areaui ' .. a[2] .. ' ' .. a[3] .. ' ', 'r', G[a[2]] and G[a[2]].children[a[3]])
  elseif c == 'roundtext' then  -- G.GAME.current_round.round_text (no box shows it)
    print('U ' .. frame .. ' roundtext [' .. tostring(G.GAME.current_round.round_text) .. ']')
  elseif c == 'pop' then
    local t = a[2] and at(a[2], a[3]) or G.CONTROLLER.focused.target
    dump_box('U ' .. frame .. ' pop ', 'r', t and t.children.h_popup)
  elseif c == 'popall' then  -- popall:<set>[:<edition, sticker, seal or debuff>]: the popup of a new card of every center of the set
    local v = a[3]
    for _, center in ipairs(G.P_CENTER_POOLS[a[2]]) do
      local card = Card(5, 5, G.CARD_W, G.CARD_H, (a[2] == 'Enhanced' or a[2] == 'Default') and G.P_CARDS.H_A or G.P_CARDS.empty, center, {bypass_discovery_center = true, bypass_discovery_ui = true})
      if v == 'foil' or v == 'holo' or v == 'polychrome' or v == 'negative' then card:set_edition({[v] = true}, true, true)
      elseif v == 'eternal' then card:set_eternal(true)
      elseif v == 'perishable' then card:set_perishable(true)
      elseif v == 'rental' then card:set_rental(true)
      elseif v == 'debuff' then card:set_debuff(true)
      elseif v then card:set_seal(v, true, true) end
      card:hover()
      dump_box('U ' .. frame .. ' pop ' .. center.key .. ' ', 'r', card.children.h_popup)
      card:stop_hover()
      card:remove()
    end
  elseif c == 'unlockall' then
    for _, t in ipairs({G.P_CENTERS, G.P_BLINDS, G.P_TAGS}) do
      for _, v in pairs(t) do v.discovered = true; v.unlocked = true; v.alerted = true end
    end
  elseif c == 'notut' then
    G.SETTINGS.tutorial_complete = true
    G.SETTINGS.tutorial_progress = nil
  elseif c == 'order' then  -- order:<area>: its cards, in order, as one "U" line
    local s = 'U ' .. frame .. ' order ' .. a[2]
    for _, k in ipairs(G[a[2]].cards) do s = s .. ' ' .. card_desc(k) .. (k.highlighted and '*' or '') end
    print(s)
  elseif c == 'tagpop' then  -- the popup of every tag
    for _, center in ipairs(G.P_CENTER_POOLS.Tag) do
      local tag = Tag(center.key, true)
      local _, sprite = tag:generate_UI()
      sprite:hover()
      dump_box('U ' .. frame .. ' pop ' .. center.key .. ' ', 'r', sprite.children.h_popup)
      sprite:stop_hover()
      sprite:remove()
    end
  elseif c == 'nf' then  -- the number formats of a list of values
    for _, v in ipairs({0, 1.5, 12.34, 123.456, 999, 1000, 12345.6, 1e6, 1234567, 12345678, 1e10, 99999999999, 1e11, 1.2345e11, 9.9995e11, 1e12, 1.5e13, 123456789e9, 1e15, 1e20, 1.797e308, 9007199254740992}) do
      print(('U %d nf %.17g %s %.4f %.4f %.4f'):format(frame, v, number_format(v), score_number_scale(1, v), scale_number(v, 1), scale_number(v, 1, 1000000)))
    end
  elseif c == 'discover' then
    discover_card(G.P_CENTERS[a[2]] or G.P_BLINDS[a[2]] or G.P_TAGS[a[2]])
  elseif c == 'unlock' then
    unlock_card(G.P_CENTERS[a[2]])
  elseif c == 'chal' then  -- chal:<i>: the description of challenge i in the open challenge list
    G.FUNCS.change_challenge_description({config = {id = tonumber(a[2])}})
  elseif c == 'fa' then  -- fa:<name>:<n>: a cycle's callback with {to_key = n} (and to_val = the deck's name, for change_viewed_back)
    G.FUNCS[a[2]]({to_key = tonumber(a[3]), to_val = G.P_CENTER_POOLS.Back[tonumber(a[3])] and G.P_CENTER_POOLS.Back[tonumber(a[3])].name})
  elseif c == 'unseed' then  -- the run counts as unseeded (discoveries and unlocks happen)
    G.GAME.seeded = nil
  elseif c == 'tabs' then  -- every tab of the open overlay (but a disabled one), chosen in turn and dumped
    local list = collect(G.OVERLAY_MENU.UIRoot, function(e) return type(e.config.id) == 'string' and e.config.id:sub(1, 8) == 'tab_but_' and e.config.button == 'change_tab' end, {})
    for _, e in ipairs(list) do
      G.FUNCS.change_tab(e)
      dump_box('U ' .. frame .. ' tab ' .. e.config.id .. ' ', 'r', G.OVERLAY_MENU)
    end
  elseif c == 'cyc' then  -- cyc:<k>:<n>: the right arrow of the k-th option cycle of the open overlay, n times, dumped after each
    for j = 1, tonumber(a[3]) do
      local list = collect(G.OVERLAY_MENU.UIRoot, function(e) return e.config.button == 'option_cycle' and e.config.ref_value == 'r' end, {})
      if not list[tonumber(a[2])] then break end
      G.FUNCS.option_cycle(list[tonumber(a[2])])
      dump_box('U ' .. frame .. ' cyc ' .. j .. ' ', 'r', G.OVERLAY_MENU)
    end
  elseif c == 'settings' then
    dump('D ' .. frame .. ' SETTINGS', G.SETTINGS, 0)
  elseif c == 'f' then
    G.FUNCS[a[2]]({config = {}})
  else
    error('hook_ui: unknown command ' .. cmd)
  end
end

local d = desc(G.CONTROLLER.focused.target)
if d ~= FOCUS_UI then
  FOCUS_UI = d
  print('F ' .. frame .. ' focus=' .. d)
end
