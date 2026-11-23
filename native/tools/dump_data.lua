-- Dumps the game's static data (the tables its Lua builds from literals) to the binary files src/data.c loads.
-- Run by tools/dump_data.sh with tools/dump_host.c (Lua 5.1.5 that remembers the order keys were defined in):
--   dump_host dump_data.lua <game dir> <out dir> <language>...
-- Only the files that define the data are loaded, with the few LOVE functions they call stubbed; nothing of the
-- game runs besides the functions named below. See include/data.h for the format and for what each section is.
local GAME, OUT = arg[1], arg[2]
local OS = os.getenv('DUMP_OS') or 'ps4' -- what love.system.getOS() answers: globals.lua sets the F_* flags by it

-- ---------------------------------------------------------------- definition order
-- The keys of t as the port iterates them: 1..n first, then the others in the order they were first set.
local function okeys(t)
    local keys, n = {}, 0
    while rawget(t, n + 1) ~= nil do n = n + 1; keys[n] = n end
    local seq, rest = keyseq(t), {}
    for k in next, t do
        if not (type(k) == 'number' and k >= 1 and k <= n and k % 1 == 0) then rest[#rest + 1] = k end
    end
    -- an integer key past a hole can sit in Lua's array part, which has no sequence: those go first, by index
    local function at(k) return seq[k] or k - 2 ^ 40 end
    table.sort(rest, function(a, b) return at(a) < at(b) end)
    for i = 1, #rest do keys[n + i] = rest[i] end
    return keys
end

-- The game's own loops (copy_table, init_game_object's bosses_used) build tables with pairs(): in stock Lua that
-- is hash order, which the port cannot reproduce. Here, as in the port, it is definition order.
function pairs(t)
    local keys, i = okeys(t), 0
    return function()
        i = i + 1
        local k = keys[i]
        if k ~= nil then return k, rawget(t, k) end
    end
end

local function skeys(t) -- numbers ascending, then strings by bytes
    local keys = okeys(t)
    table.sort(keys, function(a, b)
        if type(a) ~= type(b) then return type(a) == 'number' end
        return a < b
    end)
    return keys
end

-- ---------------------------------------------------------------- snapshot
local skipped = {} -- what is not data: {path, type, where}
local localized = {} -- the marker strings the localize() stub returned -> where it was called

local function short(src) return (src:gsub('^.*/game/', '')) end -- a path inside the game

-- A deep copy of v as it is now (later steps of the game's code must not change a section already taken), with
-- the sharing between its tables and the order of their keys kept, and without the values that are not data.
local function snap(v, path, memo)
    local ty = type(v)
    if ty == 'boolean' or ty == 'number' or ty == 'string' then
        assert(ty ~= 'string' or not v:find('%z'), path .. ': a string with a zero byte')
        if localized[v] then skipped[#skipped + 1] = path .. '	localize	' .. localized[v] end
        return v
    end
    if ty ~= 'table' then
        local where = ''
        if ty == 'function' then
            local info = debug.getinfo(v, 'S')
            where = short(info.short_src) .. ':' .. info.linedefined
        end
        skipped[#skipped + 1] = path .. '\t' .. ty .. '\t' .. where
        return nil
    end
    if memo[v] then return memo[v] end
    local copy = {}
    memo[v] = copy
    for _, k in ipairs(okeys(v)) do
        local kt = type(k)
        assert(kt == 'number' or kt == 'string', path .. ': a key of type ' .. kt)
        copy[k] = snap(rawget(v, k), path .. (kt == 'number' and '[' .. k .. ']' or '.' .. k), memo)
    end
    return copy
end

-- ---------------------------------------------------------------- output
local function u32(n)
    return string.char(n % 256, math.floor(n / 256) % 256, math.floor(n / 65536) % 256, math.floor(n / 16777216))
end

-- The binary form (include/data.h). Returns the bytes and the number of tables.
local function binary(root)
    local out, ids, ntab = {'BDT1'}, {}, 0
    local function put(v)
        local ty = type(v)
        if ty == 'boolean' then out[#out + 1] = v and '\2' or '\1'
        elseif ty == 'number' then out[#out + 1] = '\3' .. f64(v)
        elseif ty == 'string' then out[#out + 1] = '\4' .. u32(#v) .. v
        elseif ids[v] then out[#out + 1] = '\6' .. u32(ids[v])
        else
            ids[v] = ntab
            ntab = ntab + 1
            local keys = okeys(v)
            out[#out + 1] = '\5' .. u32(#keys)
            for _, k in ipairs(keys) do put(k) put(v[k]) end
        end
    end
    put(root)
    return table.concat(out), ntab
end

-- The textual form, the same data_dump() of src/data.c prints: the test compares the two.
local function text(root, keysof)
    local out, ids, ntab = {}, {}, 0
    local function scalar(v)
        if type(v) == 'number' then return string.format('%.17g', v) end
        if type(v) == 'string' then return '"' .. v .. '"' end
        return tostring(v)
    end
    local function put(v, indent)
        if type(v) ~= 'table' then out[#out + 1] = scalar(v) return end
        if ids[v] then out[#out + 1] = '@' .. ids[v] return end
        ntab = ntab + 1
        ids[v] = ntab
        out[#out + 1] = '{#' .. ntab .. '\n'
        for _, k in ipairs(keysof(v)) do
            out[#out + 1] = indent .. ' ' .. scalar(k) .. ' = '
            put(v[k], indent .. ' ')
            out[#out + 1] = '\n'
        end
        out[#out + 1] = indent .. '}'
    end
    put(root, '')
    out[#out + 1] = '\n'
    return table.concat(out)
end

local function write(name, s)
    local f = assert(io.open(OUT .. '/' .. name, 'wb'))
    f:write(s)
    f:close()
end

local function count(t) local n = 0 for _ in next, t do n = n + 1 end return n end

-- Writes <name>.dat and the two reference dumps, and prints the sizes of the tables down to `depth` levels.
local function emit(name, root, depth)
    local bytes, ntab = binary(root)
    write(name .. '.dat', bytes)
    write('ref/' .. name .. '.sorted.txt', text(root, skeys))
    write('ref/' .. name .. '.ordered.txt', text(root, okeys))
    print(string.format('%s.dat: %d bytes, %d tables', name, #bytes, ntab))
    local function sizes(t, path, level)
        for _, k in ipairs(okeys(t)) do
            if type(t[k]) == 'table' then
                print(string.format('  %s%s: %d keys', path, tostring(k), count(t[k])))
                if level < depth then sizes(t[k], path .. tostring(k) .. '.', level + 1) end
            end
        end
    end
    sizes(root, '', 1)
end

-- ---------------------------------------------------------------- the game's tables
love = {
    system = {getOS = function() return OS end},
    -- set_language: the language file "exists" (so the setting stays), no font file does (no font objects made)
    filesystem = {read = function() return true end, getInfo = function() return nil end},
    -- set_render_settings: the atlases are described, no image is loaded
    graphics = {setDefaultFilter = function() end, setLineStyle = function() end, newImage = function() return nil end},
}
dofile(GAME .. '/engine/object.lua')
dofile(GAME .. '/functions/misc_functions.lua') -- HEX, copy_table, get_starting_params
dofile(GAME .. '/game.lua')
-- A prototype holds a localized string (the language's, computed when the table is built): it is dumped as the
-- marker "localize('key')" and listed, so the port computes it after loading.
function localize(key)
    local info = debug.getinfo(2, 'Sl')
    local marker = "localize('" .. key .. "')"
    localized[marker] = short(info.short_src) .. ':' .. info.currentline
    return marker
end
dofile(GAME .. '/globals.lua') -- ends with G = Game(), which runs Game:set_globals()

local root = {}
local function fields(before) -- the fields G has now and did not have in `before`
    local t = {}
    for k, v in pairs(G) do if not before[k] then t[k] = v end end
    return t
end
local function have() local s = {} for k in next, G do s[k] = true end return s end
local function section(name, t) root[name] = snap(t, name, {}) end

G.SEED = nil -- os.time(): not data
section('globals', fields({}))

local before = have()
G:set_render_settings()
section('render', fields(before))

before = have()
G:set_language()
section('language', fields(before))

-- init_item_prototypes is stopped at its call to self:save_progress() (game.lua:729): every literal table is
-- defined by then and nothing of the profile has been read.
local STOP = {}
function Game:save_progress() error(STOP) end
before = have()
local ok, err = pcall(G.init_item_prototypes, G)
assert(not ok and err == STOP, tostring(err))
section('prototypes', fields(before))

dofile(GAME .. '/challenges.lua')
section('CHALLENGES', G.CHALLENGES)

section('GAME', G:init_game_object())

-- ---------------------------------------------------------------- literals of function bodies
-- Text the game's code has as string literals inside its functions (the credits, a few labels, the links): no table
-- holds it and the functions cannot be run here, so it is read from the source text of the user's copy. Anything
-- unexpected stops the dump: a different version of the game must be noticed here, not on the screen.
local function fail(msg)
    error('dump_data: ' .. msg .. '\n  The game\'s Lua does not have the shape this tool knows (Balatro 1.0.1o):'
        .. ' update LITERALS in tools/dump_data.lua and the C that reads it (DUMP_LITERALS=1 lists what is found).', 0)
end

-- The text of the one function whose first line starts with `header`, up to its closing `end` (the first later
-- line that starts with "end": the game writes its top-level functions that way).
local function body(file, header)
    local f = io.open(GAME .. '/' .. file, 'rb')
    if not f then fail(file .. ' cannot be read') end
    local src = '\n' .. f:read('*a')
    f:close()
    local from = src:find('\n' .. header, 1, true)
    if not from then fail(file .. ': no line starts with "' .. header .. '"') end
    if src:find('\n' .. header, from + 1, true) then fail(file .. ': two lines start with "' .. header .. '"') end
    local to = src:find('\nend', from + 1, true)
    if not to then fail(file .. ': "' .. header .. '" has no closing end') end
    return src:sub(from + 1, to)
end

-- The string literals of a piece of Lua, in order, as {s = the string, field = what it is}:
--   name        the value of `name = "..."`
--   name[]      an element of the table `name = {...}` ("[]" alone: of a table that is not a field's value)
--   [key]       an index or a table key: `["..."]`
--   f()         the first argument of a call of f
--   ...         anything else (an operand, a later argument)
-- A small tokenizer: comments, strings, names, numbers and single symbols, and the stack of open ( and {.
local function strings(text, where)
    local out, pos = {}, 1
    local prev, prev2 = '', '' -- the two tokens before the current one
    local stack = {} -- {field = } for a `{`, {call = } for a `(`
    local function tok(t) prev2, prev = prev, t end
    local function isname(t) return t:find('^[%a_]') ~= nil end
    while true do
        pos = text:find('%S', pos)
        if not pos then break end
        local c = text:sub(pos, pos)
        local long = text:match('^%-%-%[(=*)%[', pos)
        if long then
            local _, e = text:find(']' .. long .. ']', pos, true)
            if not e then fail(where .. ': a long comment that does not end') end
            pos = e + 1
        elseif text:find('^%-%-', pos) then
            pos = (text:find('\n', pos, true) or #text) + 1
        elseif c == '"' or c == "'" or text:find('^%[=*%[', pos) then
            local e
            if c == '[' then
                _, e = text:find(']' .. text:match('^%[(=*)%[', pos) .. ']', pos, true)
            else
                e = pos + 1
                while text:sub(e, e) ~= c do
                    local ch = text:sub(e, e)
                    if ch == '' or ch == '\n' then e = nil break end
                    e = e + (ch == '\\' and 2 or 1)
                end
            end
            if not e then fail(where .. ': a string that does not end: ' .. text:sub(pos, pos + 40)) end
            local s = assert(loadstring('return ' .. text:sub(pos, e)))() -- Lua's own reading of the literal
            local top, field = stack[#stack], '...'
            if prev == '=' and isname(prev2) then field = prev2
            elseif prev == '[' then field = '[key]'
            elseif prev == '(' then field = top.call .. '()'
            elseif (prev == '{' or prev == ',') and top and top.field then field = top.field .. '[]'
            end
            out[#out + 1] = {s = s, field = field}
            if os.getenv('DUMP_LITERALS') then print(string.format('  %s\t%s\t%q', where, field, s)) end
            tok('<string>')
            pos = e + 1
        elseif isname(c) then
            local name = text:match('^[%w_]+', pos)
            tok(name)
            pos = pos + #name
        elseif c:find('%d') then
            pos = pos + #text:match('^[%w.]+', pos)
            tok('<number>')
        else
            if c == '{' then stack[#stack + 1] = {field = prev == '=' and isname(prev2) and prev2 or ''}
            elseif c == '(' then stack[#stack + 1] = {call = isname(prev) and prev or ''}
            elseif c == '}' or c == ')' then
                if not stack[#stack] then fail(where .. ': a ' .. c .. ' that closes nothing') end
                stack[#stack] = nil
            end
            tok(c)
            pos = pos + 1
        end
    end
    return out
end

-- The strings of the fields in `want` (a set; a function as the value chooses among that field's strings) of a
-- function, in order; `n` is how many this version has.
local function literals(file, header, want, n)
    local out = {}
    for _, lit in ipairs(strings(body(file, header), header)) do
        local take = want[lit.field]
        if type(take) == 'function' then take = take(lit.s) end
        if take then out[#out + 1] = lit.s end
    end
    if #out ~= n then fail(file .. ': "' .. header .. '" has ' .. #out .. ' texts, not ' .. n) end
    return out
end

-- G.UIDEF.credits: its six tabs as {label = , text = {every text of the tab, in order}} and the table of the
-- collaborations the third tab builds, as an array in the order of the source ({name = , artist = , artist_link = ,
-- ml_text = {...}, art = }).
local function credits()
    local header = 'function G.UIDEF.credits('
    local layout = {align = true, button = true, func = true, opt_callback = true} -- strings that are not text
    local tabs, collabs = {}, {}
    for _, lit in ipairs(strings(body('functions/UI_definitions.lua', header), header)) do
        local f, tab, collab = lit.field, tabs[#tabs], collabs[#collabs]
        if f == 'label' then tabs[#tabs + 1] = {label = lit.s, text = {}}
        elseif (f == 'text' or f == 'label[]') and tab then tab.text[#tab.text + 1] = lit.s
        elseif f == '[key]' then collabs[#collabs + 1] = {name = lit.s}
        elseif (f == 'artist' or f == 'artist_link' or f == 'art') and collab and not collab[f] then collab[f] = lit.s
        elseif f == 'ml_text[]' and collab then
            collab.ml_text = collab.ml_text or {}
            collab.ml_text[#collab.ml_text + 1] = lit.s
        elseif not layout[f] then fail('the credits have a string this tool does not know: ' .. f .. ' = "' .. lit.s .. '"')
        end
    end
    if #tabs ~= 6 then fail('the credits have ' .. #tabs .. ' tabs, not 6') end
    if #collabs == 0 then fail('the credits have no collaborations') end
    for _, c in ipairs(collabs) do
        if not c.art then fail('the collaboration "' .. c.name .. '" of the credits has no art') end
    end
    return {tabs = tabs, collabs = collabs}
end

local UI, BC = 'functions/UI_definitions.lua', 'functions/button_callbacks.lua'
local function url(func) return literals(BC, 'G.FUNCS.' .. func .. ' = function(', {['openURL()'] = true}, 1)[1] end
root.LITERALS = snap({
    credits = credits(),
    collab_option = literals(UI, 'function G.UIDEF.viewed_collab_option(', {['[key]'] = true, ['[]'] = true, text = true}, 8),
    profile = literals('game.lua', 'function Game:load_profile(', {label = true}, 9),
    cardarea = literals('cardarea.lua', 'function CardArea:draw(', {text = true}, 6),
    win_game = literals('functions/state_events.lua', 'function win_game(', {round_text = true}, 1),
    evaluate_play = literals('functions/state_events.lua', 'G.FUNCS.evaluate_play = function(', {['play_area_status_text()'] = true}, 1),
    demo_cta = literals('game.lua', 'function Game:demo_cta(', {['string[]'] = true, ['label[]'] = true}, 3),
    exit_cta = literals(UI, 'function create_UIBox_exit_CTA(', {back_label = true}, 1),
    quit_cta = literals(BC, 'G.FUNCS.quit_cta = function(', {['[]'] = true, text = true}, 2),
    competition_name = literals(UI, 'function create_UIBox_main_menu_competittion_name(', {prompt_text = true, ['label[]'] = true}, 2),
    generic_options = literals(UI, 'function create_UIBox_generic_options(', {text = true}, 1),
    beta_lang_alert = literals(BC, 'G.FUNCS.beta_lang_alert = function(', {text = true}, 1),
    -- the sandbox's controls: three slider labels, the two options written in capitals, the prompt (the other
    -- options are keys of the code)
    sandbox = literals('game.lua', 'function Game:sandbox(',
        {label = true, prompt_text = true, ['options[]'] = function(s) return s == s:upper() end}, 6),
    -- the heading of the crash screen and its two paragraphs (the long operands of the expression that builds it)
    errhand = literals('main.lua', 'function love.errhand(', {p = true, ['...'] = function(s) return #s > 60 end}, 3),
    url = {
        demo_survey = url('demo_survey'), louisf_insta = url('louisf_insta'), wishlist_steam = url('wishlist_steam'),
        go_to_playbalatro = url('go_to_playbalatro'), go_to_discord = url('go_to_discord'),
        go_to_discord_loc = url('go_to_discord_loc'), loc_survey = url('loc_survey'), go_to_twitter = url('go_to_twitter'),
    },
}, 'LITERALS', {})

emit('game', root, 2)
for i = 3, #arg do
    local loc = assert(loadfile(GAME .. '/localization/' .. arg[i] .. '.lua'))()
    emit('loc_' .. arg[i], snap(loc, 'loc_' .. arg[i], {}), i == 3 and 2 or 0)
end
write('skipped.txt', table.concat(skipped, '\n') .. (#skipped > 0 and '\n' or ''))
print(#skipped .. ' values that are not data (data/skipped.txt)')
for _, line in ipairs(skipped) do print('  ' .. line) end
