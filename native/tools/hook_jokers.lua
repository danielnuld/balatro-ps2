-- Oracle side of the jokers comparison (HOOK=<this file>); src/hook_jokers.c does the same steps in the port.
-- Environment: CASE=j_x[+foil|holo|polychrome|negative|eternal|perishable|rental],...  the jokers added at the first blind select
--              CONS=c_pluto,c_hermit,c_strength  consumeables added there (the default)
--              SEED=JOKERS   ACT<i>=P:<cards>[|<held>] or D:<cards>  overrides round 1's action i (0-based)
--              DOLLARS=20  BLIND=1e9 (round 1's blind chips)  NOWIN=1 (the last hand is not forced to win)
--              SELLJ=<i> (the shop sells joker i instead of the last consumeable)  SKIP=1 (the big blind is skipped, the boss played)
--              <cards>: H_A+m_glass+polychrome+Red,S_K,...  (rewrites the first cards of the hand and plays/discards them;
--              <held> rewrites the last cards of the hand); an empty list takes N natural cards: P:#5
-- Every step prints "JK <step> ..." lines; tools/jokers.sh diffs them against the port's.
local frame = ...
local H = _G.__JK
if not H then
  H = {step = 0, act = 0, idle = 0, last = 0, phase = 'boot', hc = 0, hm = 0, r2 = 0}
  _G.__JK = H
end

local function split(s, sep)
  local t = {}
  for p in string.gmatch(s, '[^' .. sep .. ']+') do t[#t + 1] = p end
  return t
end
local function fv(v)
  if type(v) == 'number' then return string.format('%.10g', v) end
  if type(v) == 'boolean' then return tostring(v) end
  if type(v) == 'string' then return v end
end
local function dump(t, prefix, out)
  local keys = {}
  for k in pairs(t) do if type(k) == 'string' then keys[#keys + 1] = k end end
  table.sort(keys)
  for _, k in ipairs(keys) do
    local v = t[k]
    if type(v) == 'table' then
      if k == 'extra' then dump(v, prefix .. k .. '.', out) end
    elseif fv(v) then out[#out + 1] = prefix .. k .. '=' .. fv(v) end
  end
end
local function apply(card, spec)
  local parts = split(spec, '+')
  if G.P_CARDS[parts[1]] then card:set_base(G.P_CARDS[parts[1]]) end
  for i = 2, #parts do
    local p = parts[i]
    if G.P_CENTERS[p] then card:set_ability(G.P_CENTERS[p], nil, true)
    elseif p == 'foil' or p == 'holo' or p == 'polychrome' or p == 'negative' then card:set_edition({[p] = true}, true, true)
    elseif p == 'eternal' then card:set_eternal(true)
    elseif p == 'perishable' then card:set_perishable(true)
    elseif p == 'rental' then card:set_rental(true)
    else card:set_seal(p, true, true) end
  end
end
local function cardstr(c)
  local s = string.sub(c.base.suit or '?', 1, 1) .. tostring(c.base.id or 0)
  if c.config.center_key ~= 'c_base' then s = s .. ':' .. tostring(c.config.center_key) end
  if c.edition then s = s .. ':' .. tostring(c.edition.type) end
  if c.seal then s = s .. ':' .. c.seal end
  if (c.ability.perma_bonus or 0) ~= 0 then s = s .. ':pb' .. fv(c.ability.perma_bonus) end
  if c.debuff then s = s .. ':D' end
  return s
end
local function areasum(a)
  local s = 0
  for i, c in ipairs(a.cards) do s = s + i * ((c.base.id or 0) * 100 + string.byte(c.base.suit or '?')) end
  return #a.cards .. '/' .. s
end
local function summary(label)
  local g, cr, rr = G.GAME, G.GAME.current_round, G.GAME.round_resets
  local pr, keys = 0, {}
  for k in pairs(g.pseudorandom) do keys[#keys + 1] = k end
  table.sort(keys)
  for _, k in ipairs(keys) do if type(g.pseudorandom[k]) == 'number' then pr = pr + g.pseudorandom[k] end end
  H.step = H.step + 1
  local p = 'JK ' .. H.step .. ' ' .. label .. ' '
  print(p .. 'A st=' .. G.STATE .. ' chips=' .. fv(g.chips) .. ' hc=' .. fv(H.hc) .. ' hm=' .. fv(H.hm) .. ' $=' .. fv(g.dollars) ..
    ' round=' .. g.round .. ' ante=' .. rr.ante .. ' hands=' .. cr.hands_left .. '/' .. rr.hands .. ' disc=' .. cr.discards_left .. '/' .. rr.discards ..
    ' hsize=' .. G.hand.config.card_limit .. ' jslots=' .. G.jokers.config.card_limit .. ' cslots=' .. G.consumeables.config.card_limit ..
    ' deck=' .. areasum(G.deck) .. ' discard=' .. areasum(G.discard) .. ' all=' .. #G.playing_cards ..
    ' prob=' .. fv(g.probabilities.normal) .. ' icap=' .. fv(g.interest_cap) .. ' iamt=' .. fv(g.interest_amount) .. ' disc%=' .. fv(g.discount_percent) ..
    ' bankrupt=' .. fv(g.bankrupt_at) .. ' reroll=' .. fv(cr.reroll_cost) .. ' free=' .. fv(cr.free_rerolls) ..
    ' played=' .. fv(cr.hands_played) .. ' dused=' .. fv(cr.discards_used) .. ' skips=' .. fv(g.skips) .. ' last=' .. tostring(g.last_hand_played) ..
    ' idol=' .. tostring(cr.idol_card.rank) .. '/' .. tostring(cr.idol_card.suit) .. ' mail=' .. tostring(cr.mail_card.rank) ..
    ' ancient=' .. tostring(cr.ancient_card.suit) .. ' castle=' .. tostring(cr.castle_card.suit) .. ' pr=' .. fv(pr) .. ' f=' .. frame)
  for i, c in ipairs(G.jokers.cards) do
    local out = {}
    dump(c.ability, '', out)
    print(p .. 'J' .. i .. ' ' .. tostring(c.config.center_key) .. ' ed=' .. tostring(c.edition and c.edition.type) .. ' sell=' .. fv(c.sell_cost) ..
      ' cost=' .. fv(c.cost) .. ' debuff=' .. tostring(c.debuff) .. ' ' .. table.concat(out, ','))
  end
  local t = {}
  for _, c in ipairs(G.consumeables.cards) do t[#t + 1] = tostring(c.config.center_key) .. (c.edition and (':' .. c.edition.type) or '') end
  print(p .. 'C ' .. table.concat(t, ','))
  t = {}
  for _, c in ipairs(G.hand.cards) do t[#t + 1] = cardstr(c) end
  print(p .. 'H ' .. table.concat(t, ' '))
  t = {}
  for _, h in ipairs(G.handlist) do
    local v = g.hands[h]
    t[#t + 1] = fv(v.level) .. ':' .. fv(v.chips) .. 'x' .. fv(v.mult) .. ':' .. fv(v.played)
  end
  print(p .. 'L ' .. table.concat(t, ' '))
  if G.shop_jokers and G.shop_jokers.cards then
    t = {}
    for _, a in ipairs({G.shop_jokers, G.shop_vouchers, G.shop_booster}) do
      for _, c in ipairs(a.cards) do t[#t + 1] = tostring(c.config.center_key) .. (c.edition and (':' .. c.edition.type) or '') .. '$' .. fv(c.cost) end
    end
    print(p .. 'S ' .. table.concat(t, ' '))
  end
  io.stdout:flush()
  H.hc, H.hm = 0, 0
end
local function finish(msg)
  print('JK END ' .. msg .. ' st=' .. tostring(G.STATE))
  io.stdout:flush()
  os.exit(0)
end

-- the actions of round 1, in order; a discard with none left is skipped, the last hand is forced to win
local ACTS = {
  'P:H_A,H_2,H_3,H_4,H_5',
  'D:H_J,C_J,D_K',
  'U',  -- use the first consumeable
  'P:S_K,C_K,D_K,H_Q,C_Q|S_K,S_Q',
  'D:#1',
  'U',
  'P:S_T,H_T,D_T,C_T',
  'D:#5',
  'P:S_6,D_6,C_9',
  'P:#5', 'P:#5', 'P:#5', 'P:#5', 'P:#5', 'P:#5',
}
for i = 0, #ACTS - 1 do if os.getenv('ACT' .. i) then ACTS[i + 1] = os.getenv('ACT' .. i) end end

-- rewrites and highlights the cards of an action; returns false if there are no cards
local function pick(spec)
  local parts = split(spec, '|')
  local n = #G.hand.cards
  if parts[2] then
    local held = split(parts[2], ',')
    for i, s in ipairs(held) do if G.hand.cards[n - #held + i] then apply(G.hand.cards[n - #held + i], s) end end
  end
  local list = parts[1] or '#5'
  local count
  if string.sub(list, 1, 1) == '#' then count = tonumber(string.sub(list, 2))
  else
    local cs = split(list, ',')
    count = #cs
    for i, s in ipairs(cs) do if G.hand.cards[i] then apply(G.hand.cards[i], s) end end
  end
  local picked = {}
  for i = 1, math.min(count, n) do picked[i] = G.hand.cards[i] end
  for _, c in ipairs(picked) do G.hand:add_to_highlighted(c) end
  return #picked > 0
end

if frame == 60 then
  G.SETTINGS.tutorial_complete = true
  G.SETTINGS.tutorial_progress = nil
  G.SETTINGS.GAMESPEED = 4
  G.FUNCS.start_run(nil, {stake = tonumber(os.getenv('STAKE') or '1'), seed = os.getenv('SEED') or 'JOKERS'})
  H.phase = 'select1'
  H.last = frame
  return
end
if frame < 60 or not G.GAME or not G.GAME.current_round or not G.hand then return end

local ch = G.GAME.current_round.current_hand
if ch and type(ch.chips) == 'number' and ch.chips ~= 0 then H.hc = ch.chips end
if ch and type(ch.mult) == 'number' and ch.mult ~= 0 then H.hm = ch.mult end

if G.STATE == G.STATES.GAME_OVER then summary('gameover'); finish('gameover') end
if frame - H.last > 8000 then summary('timeout'); finish('timeout ' .. H.phase .. ' complete=' .. tostring(G.STATE_COMPLETE) .. ' locked=' .. tostring(G.CONTROLLER.locked)) end

-- ready: the state has settled and nothing is queued, for 10 frames
local busy = 0
for _, e in ipairs(G.E_MANAGER.queues.base) do if e.blocking then busy = busy + 1 end end
if busy == 0 and G.STATE_COMPLETE and not G.CONTROLLER.locked then H.idle = H.idle + 1 else H.idle = 0 end
local st = G.STATE
local function acted() H.idle = 0; H.last = frame end
-- the round evaluation keeps an event queued: its cash out button, for 30 frames, is the signal there
local btn
if st == G.STATES.ROUND_EVAL and G.round_eval then  -- the button is in a UIBox of its own (common_events.lua:1069)
  for _, b in ipairs(G.I.UIBOX) do btn = btn or b:get_UIE_by_ID('cash_out_button') end
end
if btn then H.evalf = (H.evalf or 0) + 1 else H.evalf = 0 end
if H.phase == 'round1' and btn and H.evalf >= 30 then
  summary('eval')
  G.FUNCS.cash_out(btn)
  H.phase = 'shop'; acted()
  return
end
if H.phase == 'round2' and btn and H.evalf >= 30 then summary('eval2'); finish('done') end
if H.idle < 10 then return end

if H.phase == 'select1' and st == G.STATES.BLIND_SELECT and G.blind_select then
  G.GAME.dollars = tonumber(os.getenv('DOLLARS') or '20')
  G.consumeables.config.card_limit = 3
  for _, spec in ipairs(split(os.getenv('CASE') or '', ',')) do
    local parts = split(spec, '+')
    local card = create_card('Joker', G.jokers, nil, nil, nil, nil, parts[1])
    apply(card, spec)
    card:add_to_deck()
    G.jokers:emplace(card)
  end
  for _, key in ipairs(split(os.getenv('CONS') or 'c_pluto,c_hermit,c_strength', ',')) do
    local card = create_card('Tarot_Planet', G.consumeables, nil, nil, nil, nil, key)
    card:add_to_deck()
    G.consumeables:emplace(card)
  end
  summary('start')
  G.FUNCS.select_blind(G.blind_select_opts.small:get_UIE_by_ID('select_blind_button'))
  H.phase = 'round1'; acted()
elseif H.phase == 'round1' and st == G.STATES.SELECTING_HAND and #G.hand.cards > 0 then
  if H.act == 0 then G.GAME.blind.chips = tonumber(os.getenv('BLIND') or '1e9') end
  summary('r1a' .. H.act)
  while true do
    local a = ACTS[H.act + 1] or 'P:#5'
    H.act = H.act + 1
    local kind = string.sub(a, 1, 1)
    if G.GAME.current_round.hands_left <= 1 and kind ~= 'P' then
      -- the last hand is played at once
    elseif kind == 'U' then
      local c = G.consumeables.cards[1]
      if c and c:can_use_consumeable() then G.FUNCS.use_card({config = {ref_table = c}}); break end
    elseif kind == 'D' then
      if G.GAME.current_round.discards_left > 0 and pick(string.sub(a, 3)) then G.FUNCS.discard_cards_from_highlighted(); break end
    else
      if G.GAME.current_round.hands_left <= 1 and not os.getenv('NOWIN') then G.GAME.chips = G.GAME.blind.chips end
      pick(string.sub(a, 3))
      G.FUNCS.play_cards_from_highlighted()
      break
    end
  end
  acted()
elseif H.phase == 'shop' and st == G.STATES.SHOP and G.shop then
  summary('shop')
  local c = G.consumeables.cards[#G.consumeables.cards]
  if os.getenv('SELLJ') then c = G.jokers.cards[tonumber(os.getenv('SELLJ'))] end
  if c then G.FUNCS.sell_card({config = {ref_table = c}}) end
  H.phase = 'shop2'; acted()
elseif H.phase == 'shop2' and st == G.STATES.SHOP and G.shop then
  summary('sold')
  if G.GAME.dollars - G.GAME.bankrupt_at >= G.GAME.current_round.reroll_cost then G.FUNCS.reroll_shop() end
  H.phase = 'shop3'; acted()
elseif H.phase == 'shop3' and st == G.STATES.SHOP and G.shop then
  summary('rerolled')
  G.FUNCS.toggle_shop()
  H.phase = 'select2'; acted()
elseif H.phase == 'select2' and st == G.STATES.BLIND_SELECT and G.blind_select then
  summary('select2')
  if os.getenv('SKIP') then
    G.FUNCS.skip_blind(G.blind_select_opts.big:get_UIE_by_ID('select_blind_button'))
    H.phase = 'select3'
  else
    G.FUNCS.select_blind(G.blind_select_opts.big:get_UIE_by_ID('select_blind_button'))
    H.phase = 'round2'
  end
  acted()
elseif H.phase == 'select3' and st == G.STATES.BLIND_SELECT and G.blind_select then
  summary('select3')
  G.FUNCS.select_blind(G.blind_select_opts.boss:get_UIE_by_ID('select_blind_button'))
  H.phase = 'round2'; acted()
elseif H.phase == 'round2' and st == G.STATES.SELECTING_HAND and #G.hand.cards > 0 then
  summary('r2a' .. H.r2)
  H.r2 = H.r2 + 1
  if H.r2 == 1 then G.GAME.blind.chips = 1e9; pick('#5'); G.FUNCS.play_cards_from_highlighted()
  elseif H.r2 == 2 and G.GAME.current_round.discards_left > 0 then pick('#2'); G.FUNCS.discard_cards_from_highlighted()
  else finish('done') end
  acted()
end
