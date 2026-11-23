-- Test hook of the blinds comparison (host only): the oracle's side of src/hook_blinds.c. Called every frame.
-- It drives a run by state (blind select, hand, round eval, shop, packs) and prints a CMP line before every action.
-- Environment: SEED, BOSS (G.FORCE_BOSS), FTAG (G.FORCE_TAG), PLAN (one char per blind offered: p play, s skip, r reroll the boss; the last
-- repeats), HAND (per blind: p play 5, d discard 3, w win, l lose, x blind:disable(), S sell joker 1; then w),
-- i run info, v deck view (each closed by the next step), ANTE, MONEY, JOKERS (comma list), UNSEED, SPEED, STAKE,
-- NOAUTO (only start the run and print the ticks: the pad script plays).
local frame = ...
local function env(k) local v = os.getenv(k); if v == '' then v = nil end; return v end
local H = BL_HOOK
if not H then H = {st = -1, since = 0, blinds = 0, step = 0, shop = -1, setup = false}; BL_HOOK = H end

local function n(x) if x == nil then return 'nil' end; return string.format('%.10g', x) end
local function cards(area)
  local s = {}
  if area and area.cards then
    for i = 1, #area.cards do
      local c = area.cards[i]
      s[#s + 1] = (c.config.card_key or c.config.center_key or '?')..(c.debuff and 'd' or '')..(c.facing == 'back' and 'b' or '')..(c.highlighted and 'h' or '')
    end
  end
  return table.concat(s, ',')
end
local function shopc(area)
  local s = {}
  if area and area.cards then
    for i = 1, #area.cards do
      local c = area.cards[i]
      s[#s + 1] = (c.config.center_key or '?')..':'..n(c.cost)..(c.edition and (':'..tostring(c.edition.type)) or '')
    end
  end
  return table.concat(s, ',')
end
local function cmp(label)
  local g = G.GAME
  local tags, lv = {}, 0
  for i = 1, #g.tags do tags[#tags + 1] = g.tags[i].key end
  for _, v in pairs(g.hands) do lv = lv + v.level end
  local rr, cr, b = g.round_resets, g.current_round, g.blind
  print(string.format('CMP %d %s st=%d ante=%s round=%s chips=%s bchips=%s $=%s hs=%s hl=%s dl=%s deck=%d pc=%d lv=%s blind=%s dis=%s boss=%s bs=%s/%s/%s bt=%s/%s won=%s tags=[%s] jk=[%s] cons=[%s] hand=[%s] sj=[%s] sv=[%s] sb=[%s] rr=%s',
    frame, label, G.STATE, n(rr.ante), n(g.round), n(g.chips), n(b and b.chips), n(g.dollars), n(G.hand and G.hand.config.card_limit),
    n(cr.hands_left), n(cr.discards_left), G.deck and #G.deck.cards or -1, G.playing_cards and #G.playing_cards or -1, n(lv),
    tostring(b and b.name), tostring(b and b.disabled and true or false), tostring(rr.blind_choices.Boss),
    tostring(rr.blind_states.Small), tostring(rr.blind_states.Big), tostring(rr.blind_states.Boss),
    tostring(rr.blind_tags and rr.blind_tags.Small), tostring(rr.blind_tags and rr.blind_tags.Big), tostring(g.won and true or false),
    table.concat(tags, ','), cards(G.jokers), cards(G.consumeables), cards(G.hand),
    shopc(G.shop_jokers), shopc(G.shop_vouchers), shopc(G.shop_booster), n(cr.reroll_cost)))
end
local function pick(k)
  for i = 1, #G.hand.cards do
    if #G.hand.highlighted >= k then break end
    if not G.hand.cards[i].highlighted then G.hand:add_to_highlighted(G.hand.cards[i], true) end
  end
end

if frame == 1300 then
  G.SETTINGS.tutorial_complete = true
  G.SETTINGS.tutorial_progress = nil -- as state_events.lua:1213; else it forces the first two tags
  if env('SPEED') then G.SETTINGS.GAMESPEED = tonumber(env('SPEED')) end
  G.FORCE_BOSS = env('BOSS')
  G.FORCE_TAG = env('FTAG')
  G.FUNCS.start_run(nil, {stake = tonumber(env('STAKE') or '1'), seed = env('SEED') or 'BLINDS'})
  H.started = true
  return
end
if not H.started or not G.GAME or not G.hand or not G.GAME.round_resets then return end
if G.STATE ~= H.st then H.st = G.STATE; H.since = frame end
if frame % 300 == 0 then cmp('tick') end
if env('DBG') and frame >= tonumber(env('DBG')) and frame < tonumber(env('DBG')) + 30 then -- the clocks and the queue, to tell a tie from a bug
  if env('DBGQ') then
    local q = {}
    for i = 1, math.min(6, #G.E_MANAGER.queues.base) do local v = G.E_MANAGER.queues.base[i]; q[#q + 1] = string.format('%s:%s:%.5f:%.3f:%s%s', v.trigger, v.timer, v.time, v.delay, v.blocking and 'B' or '-', v.start_timer and 'S' or '-') end
    print('DBGQ '..frame..' '..table.concat(q, ' '))
  end
  local e = nil -- the first timed event of the queue
  for i = 1, #G.E_MANAGER.queues.base do if not e and G.E_MANAGER.queues.base[i].delay > 0 then e = G.E_MANAGER.queues.base[i] end end
  print(string.format('DBG %d total=%.6f real=%.6f base=%d head=%s/%s time=%.9f delay=%.17g', frame, G.TIMERS.TOTAL, G.TIMERS.REAL, #G.E_MANAGER.queues.base,
    e and e.trigger or '-', e and e.timer or '-', e and e.time or 0, e and e.delay or 0))
end
local wait = frame - H.since
if env('NOAUTO') then return end
local S = G.STATES

if G.STATE == S.BLIND_SELECT and wait >= 300 and G.blind_select and G.blind_select_opts and not G.OVERLAY_MENU then
  if not H.setup then
    H.setup = true
    if env('UNSEED') then G.GAME.seeded = false end
    if env('ANTE') then G.GAME.round_resets.ante = tonumber(env('ANTE')); G.GAME.round_resets.blind_ante = tonumber(env('ANTE')) end
    if env('MONEY') then G.GAME.dollars = tonumber(env('MONEY')) end
    if env('JOKERS') then for k in string.gmatch(env('JOKERS'), '[^,]+') do add_joker(k) end end
  end
  local plan = env('PLAN') or 'p'
  H.dec = (H.dec or 0) + 1
  local i = math.min(H.dec, #plan)
  local c = string.sub(plan, i, i)
  if G.GAME.blind_on_deck == 'Boss' and c == 's' then c = 'p' end
  local opt = G.blind_select_opts[string.lower(G.GAME.blind_on_deck)]
  cmp('select_'..c..'_'..G.GAME.blind_on_deck)
  if c == 'r' then -- the boss reroll of the Retcon voucher
    G.GAME.used_vouchers.v_retcon = true
    G.FUNCS.reroll_boss(nil)
  elseif c == 's' then
    G.FUNCS.skip_blind(opt:get_UIE_by_ID('tag_container'))
  else
    G.FUNCS.select_blind(opt:get_UIE_by_ID('select_blind_button'))
    H.blinds = H.blinds + 1
    H.step = 0
  end
  H.since = frame
elseif G.STATE == S.SELECTING_HAND and wait >= 180 and #G.hand.cards > 0 and (not G.OVERLAY_MENU or H.info) then
  local seq = env('HAND') or 'pdw'
  H.step = H.step + 1
  local c = H.step <= #seq and string.sub(seq, H.step, H.step) or 'w'
  if H.info then c = 'o' end
  cmp('hand_'..c)
  if c == 'p' then
    pick(5); G.FUNCS.play_cards_from_highlighted(nil)
  elseif c == 'd' then
    if G.GAME.current_round.discards_left > 0 then pick(3); G.FUNCS.discard_cards_from_highlighted(nil) end
  elseif c == 'w' then
    G.GAME.chips = G.GAME.blind.chips
    pick(5); G.FUNCS.play_cards_from_highlighted(nil)
  elseif c == 'l' then
    G.GAME.chips = 0
    G.GAME.current_round.hands_left = 1
    pick(1); G.FUNCS.play_cards_from_highlighted(nil)
  elseif c == 'x' then
    G.GAME.blind:disable()
  elseif c == 'S' then
    if G.jokers.cards[1] then G.jokers.cards[1]:sell_card() end
  elseif c == 'i' then
    H.info = true; G.FUNCS.run_info()
  elseif c == 'v' then
    H.info = true; G.FUNCS.deck_info()
  elseif c == 'o' then
    H.info = false; H.step = H.step - 1; G.FUNCS.exit_overlay_menu()
  end
  H.since = frame
elseif G.STATE == S.ROUND_EVAL and wait >= 120 then
  if G.OVERLAY_MENU then
    if wait >= 400 and not H.won then
      H.won = true
      cmp('win')
      G.FUNCS.exit_overlay_menu()
      H.since = frame
    end
  elseif G.round_eval then
    local e = nil -- the button is a box of its own (common_events.lua:1069)
    for i = 1, #G.I.UIBOX do
      local b = G.I.UIBOX[i]:get_UIE_by_ID('cash_out_button')
      if b and b.config.button then e = b end
    end
    if e and e.config.button then
      cmp('cashout')
      G.FUNCS.cash_out(e)
      H.since = frame
    end
  end
elseif G.STATE == S.SHOP and wait >= 300 and G.shop and H.shop ~= H.blinds and not G.OVERLAY_MENU then
  H.shop = H.blinds
  cmp('shop')
  G.FUNCS.toggle_shop()
  H.since = frame
elseif (G.STATE == S.TAROT_PACK or G.STATE == S.PLANET_PACK or G.STATE == S.SPECTRAL_PACK or G.STATE == S.STANDARD_PACK or G.STATE == S.BUFFOON_PACK)
    and wait >= 300 and G.pack_cards and not G.OVERLAY_MENU then
  cmp('pack')
  G.FUNCS.skip_booster(nil)
  H.since = frame
elseif G.STATE == S.GAME_OVER and wait >= 300 and not H.over then
  H.over = true
  cmp('gameover')
end
