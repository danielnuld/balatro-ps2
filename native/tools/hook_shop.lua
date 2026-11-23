-- Test hook of the shop comparison (oracle side; src/hook_shop.c does the same steps in the port).
-- STEPS="1250:start:SEED 1500:blind 1700:give:c_fool ..." in the environment: at each frame, one command.
-- Commands: start:<seed>[:<stake>[:<deck key>]] | play | discard | chips:<n> | blind | win | cash | money:<n> | shop:<center key>[:<P_CARDS key>] | buy:<area>:<i> |
-- use:<area>:<i> (i 0: the last card) | sell:<area>:<i> | reroll | skip | leave | give:<center key> | hl:<area>:<i>[:<j>...] | unhl:<area> |
-- canuse:<area>:<i> | focus | hover:<area>:<i> | unhover:<area>:<i> | set:<key of G.GAME>:<number> | dump (G.GAME and every card of every area, as sorted "D" lines)
local frame = ...
if not STEPS_SHOP then
  STEPS_SHOP = {}
  for at, cmd in (os.getenv("STEPS") or ""):gmatch("(%d+):(%S+)") do
    STEPS_SHOP[tonumber(at)] = STEPS_SHOP[tonumber(at)] or {}
    table.insert(STEPS_SHOP[tonumber(at)], cmd)
  end
end
if not STEPS_SHOP[frame] then return end

local AREAS = {'hand', 'jokers', 'consumeables', 'shop_jokers', 'shop_booster', 'shop_vouchers', 'pack_cards', 'play', 'deck', 'discard'}

local function dump(pre, v, depth)
  local t = type(v)
  if t == 'number' then print(pre .. '=' .. ('%.10g'):format(v))
  elseif t == 'string' or t == 'boolean' then print(pre .. '=' .. tostring(v))
  elseif t == 'table' then
    if getmetatable(v) then print(pre .. '=<obj>'); return end
    if depth > 8 then return end
    for k, x in pairs(v) do
      if not (type(k) == 'string' and (k:find('colour') or k == 'example')) then dump(pre .. '.' .. tostring(k), x, depth + 1) end
    end
  end
end

local function dump_all()
  local p = 'D ' .. frame .. ' '
  print(p .. 'STATE=' .. tostring(G.STATE))
  dump(p .. 'GAME', G.GAME, 0)
  for _, name in ipairs(AREAS) do
    local a = G[name]
    if a and a.cards then
      print(p .. name .. '.n=' .. #a.cards)
      print(p .. name .. '.card_limit=' .. tostring(a.config.card_limit))
      print(p .. name .. '.highlighted=' .. #a.highlighted)
      if name ~= 'deck' and name ~= 'discard' then
        for i, c in ipairs(a.cards) do
          local q = p .. name .. '.' .. i
          print(q .. '.center=' .. tostring(c.config.center.key))
          dump(q .. '.ability', c.ability, 0)
          dump(q .. '.base', c.base, 0)
          dump(q .. '.edition', c.edition, 0)
          print(q .. '.seal=' .. tostring(c.seal))
          print(q .. '.cost=' .. tostring(c.cost))
          print(q .. '.sell_cost=' .. tostring(c.sell_cost))
          print(q .. '.debuff=' .. tostring(c.debuff and true or false))
          print(q .. '.facing=' .. tostring(c.facing))
        end
      else
        local s = ''
        for i, c in ipairs(a.cards) do s = s .. ' ' .. tostring(c.base.suit):sub(1, 1) .. tostring(c.base.id) .. (c.config.center.key ~= 'c_base' and c.config.center.key or '') end
        print(p .. name .. '.cards=' .. s)
      end
    end
  end
end

local function at(area, i) i = tonumber(i); return G[area].cards[i == 0 and #G[area].cards or i] end

local function split(s)
  local r = {}
  for w in s:gmatch("[^:]+") do r[#r + 1] = w end
  return r
end

for _, cmd in ipairs(STEPS_SHOP[frame]) do
  local a = split(cmd)
  local c = a[1]
  print('STEP ' .. frame .. ' ' .. cmd .. ' state ' .. tostring(G.STATE))
  if c == 'start' then
    G.SETTINGS.tutorial_complete = true
    G.SETTINGS.tutorial_progress = nil
    if a[4] then G.GAME.viewed_back = Back(G.P_CENTERS[a[4]]) end  -- the deck, as the run setup screen sets it
    G.FUNCS.start_run(nil, {stake = tonumber(a[3]) or 1, seed = a[2]})
  elseif c == 'blind' then
    G.FUNCS.select_blind(G.blind_select_opts[string.lower(G.GAME.blind_on_deck)]:get_UIE_by_ID('select_blind_button'))
  elseif c == 'win' then
    G.GAME.chips = G.GAME.blind.chips
    G.STATE = G.STATES.HAND_PLAYED
    G.STATE_COMPLETE = false
  elseif c == 'cash' then
    G.FUNCS.cash_out({config = {}})
  elseif c == 'money' then
    ease_dollars(tonumber(a[2]), true)
  elseif c == 'set' then
    G.GAME[a[2]] = tonumber(a[3])
  elseif c == 'shop' then
    local center = G.P_CENTERS[a[2]]
    if center.set == 'Booster' then
      local card = Card(G.shop_booster.T.x + G.shop_booster.T.w/2, G.shop_booster.T.y, G.CARD_W*1.27, G.CARD_H*1.27, G.P_CARDS.empty, center, {bypass_discovery_center = true, bypass_discovery_ui = true})
      create_shop_card_ui(card, 'Booster', G.shop_booster)
      card:start_materialize()
      G.shop_booster:emplace(card)
    elseif center.set == 'Voucher' then
      G.shop_vouchers.config.card_limit = G.shop_vouchers.config.card_limit + 1
      local card = Card(G.shop_vouchers.T.x + G.shop_vouchers.T.w/2, G.shop_vouchers.T.y, G.CARD_W, G.CARD_H, G.P_CARDS.empty, center, {bypass_discovery_center = true, bypass_discovery_ui = true})
      create_shop_card_ui(card, 'Voucher', G.shop_vouchers)
      card:start_materialize()
      G.shop_vouchers:emplace(card)
    else
      local card = Card(G.shop_jokers.T.x + G.shop_jokers.T.w/2, G.shop_jokers.T.y, G.CARD_W, G.CARD_H, a[3] and G.P_CARDS[a[3]] or G.P_CARDS.empty, center, {bypass_discovery_center = true, bypass_discovery_ui = true})
      create_shop_card_ui(card, center.set, G.shop_jokers)
      card:start_materialize()
      G.shop_jokers:emplace(card)
    end
  elseif c == 'give' then
    local center = G.P_CENTERS[a[2]]
    local area = center.set == 'Joker' and G.jokers or G.consumeables
    local card = create_card(center.set, area, nil, nil, nil, nil, a[2])
    card:add_to_deck()
    area:emplace(card)
  elseif c == 'buy' then
    G.FUNCS.buy_from_shop({config = {ref_table = at(a[2], a[3])}})
  elseif c == 'use' then
    local card = at(a[2], a[3])
    if card.ability.consumeable and not card:can_use_consumeable() then print('D ' .. frame .. ' use=no')  -- as the button, which is greyed
    else G.FUNCS.use_card({config = {ref_table = card}}) end
  elseif c == 'sell' then
    G.FUNCS.sell_card({config = {ref_table = at(a[2], a[3])}})
  elseif c == 'canuse' then
    local card = at(a[2], a[3])
    print('D ' .. frame .. ' canuse=' .. tostring(card.ability.consumeable and card:can_use_consumeable() and true or false))
  elseif c == 'hl' then
    for i = 3, #a do G[a[2]]:add_to_highlighted(at(a[2], a[i]), true) end
  elseif c == 'unhl' then
    G[a[2]]:unhighlight_all()
  elseif c == 'reroll' then
    G.FUNCS.reroll_shop()
  elseif c == 'skip' then
    if G.booster_pack then G.FUNCS.skip_booster() end
  elseif c == 'leave' then
    G.FUNCS.toggle_shop()
  elseif c == 'chips' then  -- the score the blind asks for
    G.GAME.blind.chips = tonumber(a[2])
  elseif c == 'play' then
    G.FUNCS.play_cards_from_highlighted()
  elseif c == 'discard' then
    G.FUNCS.discard_cards_from_highlighted()
  elseif c == 'hover' then  -- the card's popup (its description), as the cursor makes it
    at(a[2], a[3]):hover()
  elseif c == 'unhover' then
    at(a[2], a[3]):stop_hover()
  elseif c == 'focus' then  -- what the pad's cursor is on
    local t, s = G.CONTROLLER.focused.target, 'nil'
    if t and t:is(Card) then
      s = 'card ? ' .. tostring(t.rank) .. ' ' .. tostring(t.config.center.key)
      for _, name in ipairs(AREAS) do if G[name] and G[name] == t.area then s = 'card ' .. name .. ' ' .. tostring(t.rank) .. ' ' .. tostring(t.config.center.key) end end
    elseif t and t:is(UIElement) then s = 'ui ' .. tostring(t.config.button) .. ' ' .. tostring(t.config.id)
    elseif t then s = 'other' end
    print('D ' .. frame .. ' focus=' .. s)
  elseif c == 'dump' then
    dump_all()
  else
    error('hook_shop: unknown command ' .. cmd)
  end
end
