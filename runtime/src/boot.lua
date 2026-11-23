-- The love module, headless (phase 1): enough of LÖVE 11 for the game's Lua to load, run love.load and step
-- update / draw with nothing on screen. Anything the game touches that is not here aborts with its name.
local GAME, FRAMES, NUMBER_SIZE, MODE, SHOTS = ...

local function clock() local s, f = sys.clock() return s + f end

-- math.random is LuaJIT's (main.c); these are the numbers the game's own lua51.dll gives for these seeds
math.randomseed(0.5)
assert(math.random() == 0.09657393438653461 and math.random() == 0.96226945770684003 and math.random(52) == 28 and
       math.random(10, 20) == 11, "math.random differs from LuaJIT's")
math.randomseed(1700000000)
assert(math.random() == 0.66970184905361352, "math.random differs from LuaJIT's")

local absent = {dis = true}  -- names LÖVE itself lacks that the game reads as nil (misc_functions.lua:109)
local function module(name, t)
  return setmetatable(t, {__index = function(_, k)
    if not absent[k] then error(("love.%s.%s is not implemented"):format(name, k), 2) end
  end})
end
-- LÖVE's objects are userdata: the game tells its own tables apart with type(v) == "table" (game.lua:1175)
local function object(kind, t)
  local u = newproxy(true)
  getmetatable(u).__index = function(_, k)
    local v = t[k]
    if v == nil then error(("%s:%s is not implemented"):format(kind, k), 2) end
    return v
  end
  return u
end
local function nop() end

love = {arg = {parseGameArguments = function() return {} end}}
arg = {}

---------------------------------------------------------------- filesystem
-- The game's files come from files.txt (written by tools/extract.py): no directory calls needed on any device.
-- What the game writes (settings, profiles, the run in progress) goes to the memory card through save.c when the
-- game is played (MODE "play") or a test asks for it (MODE "saves", in a directory of its own, or
-- SAVE_DIR=<directory> on the host); otherwise it stays in memory, so benches and tests start every time from a
-- new profile.
-- saves: path -> the contents, or false for a file known not to be there (the card is slow to ask).
local files, dirs, saves = {}, {[""] = {}}, {}
local SAVE_DIR = os.getenv("SAVE_DIR") or
  (sys.ee and (MODE == "play" and "mc0:/BALATRO" or MODE == "saves" and "mc0:/BALATEST"))
local CARD = SAVE_DIR and save.open(SAVE_DIR)
if SAVE_DIR then print(("SAVE %s %s"):format(SAVE_DIR, CARD and "ready" or "CANNOT BE WRITTEN: nothing will be saved")) end
local function saved(path)
  if saves[path] == nil then saves[path] = CARD and save.read(path) or false end
  return saves[path]
end
local card_time = 0  -- seconds spent writing to the card, for the SAVE lines
local function store(path, data)
  saves[path] = data
  local t = clock()
  if CARD and not save.write(path, data) then
    saves[path] = nil  -- whatever the card holds now
    print("SAVE failed to write " .. path)
    return false
  end
  card_time = card_time + clock() - t
  return true
end
for line in assert(sys.read(GAME .. "/files.txt"), "no game files in " .. GAME):gmatch("[^\r\n]+") do
  local size, path = line:match("^(%d+) (.+)$")
  files[path] = tonumber(size)
  local child = path
  for dir in function() child = child:match("^(.*)/[^/]*$") return child end do
    dirs[dir] = dirs[dir] or {}
  end
end
for path in pairs(files) do
  local dir, name = path:match("^(.*)/([^/]*)$")
  table.insert(dirs[dir or ""], name or path)
end
for dir in pairs(dirs) do
  local parent, name = dir:match("^(.*)/([^/]*)$")
  if dir ~= "" then table.insert(dirs[parent or ""], name or dir) end
end
for _, list in pairs(dirs) do table.sort(list) end

local function read_game(path, bytes)
  return sys.read(GAME .. "/" .. path, bytes)
end

love.filesystem = module("filesystem", {
  read = function(path)
    local data = saved(path) or (files[path] and read_game(path))
    if not data then return nil, "Could not open file " .. path end
    return data, #data
  end,
  write = store,
  append = function(path, data) return store(path, (saved(path) or "") .. data) end,
  remove = function(path)
    saves[path] = false
    if CARD then save.remove(path) end
    return true
  end,
  createDirectory = function(path)
    if CARD then save.mkdir(path) end
    return true
  end,
  getInfo = function(path)
    if saved(path) then return {type = "file", size = #saves[path]} end
    if files[path] then return {type = "file", size = files[path]} end
    if dirs[path] then return {type = "directory"} end
  end,
  getDirectoryItems = function(path) return dirs[path] or {} end,
  getSourceBaseDirectory = function() return GAME end,
  load = function(path)
    local data = read_game(path)
    if not data then return nil, "Could not open file " .. path end
    return loadstring(data, "@" .. path)
  end,
})

table.insert(package.loaders, 2, function(name)
  local path = name .. ".lua"
  if not files[path] then return "\n\tno file '" .. path .. "' in the game" end
  return assert(love.filesystem.load(path))
end)

---------------------------------------------------------------- timer and events
-- dt is the measured one in MODE "play" (and for the bot of MODE "pad" on the EE, where a frame is long); the
-- benches and tests step a fixed 1/60 s per frame (DT=<seconds> in the environment for another), so a run does the
-- same on every machine. getTime is the real clock: the game budgets its GC steps with it (nuGC).
local frame = 0
local BOT = MODE == "pad" or MODE == "saves"
local MEASURED = MODE == "play" or (BOT and sys.ee ~= nil)
local last_step, dt = nil, tonumber(os.getenv("DT")) or 1 / 60
love.timer = module("timer", {
  getTime = clock,
  step = function()
    if MEASURED then
      local t = clock()
      dt, last_step = t - (last_step or t), t
    end
    return dt
  end,
  getFPS = function() return math.floor(1 / math.max(dt, 1 / 60) + 0.5) end,
  sleep = nop,  -- the frame is paced by the vsync of present
})

-- The pad (pad.c) as LÖVE's one gamepad: pump reads it and queues what changed as LÖVE's events.
-- The names are SDL's, as LÖVE gives them; in pad.c's bit order. SELECT is "guide": with getOS() == 'ps4' the
-- game puts there what the PC build has on "back" (G.F_GUIDE). L2 and R2 are LÖVE's trigger axes.
local BUTTONS = {"guide", "leftstick", "rightstick", "start", "dpup", "dpright", "dpdown", "dpleft",
                 "triggerleft", "triggerright", "leftshoulder", "rightshoulder", "y", "b", "a", "x"}
local AXES = {"leftx", "lefty", "rightx", "righty"}
local held, axes, events = {}, {triggerleft = 0, triggerright = 0}, {}
local gamepad = object("Joystick", {
  isGamepad = function() return true end,
  -- the game takes the pad's name from here and shows PlayStation glyphs for "PS<digit>" (controller.lua:119)
  getGamepadMappingString = function() return "0,PS2 Controller," end,
  getGamepadAxis = function(_, axis) return axes[axis] end,
  setVibration = nop,  -- ponytail: no rumble; the DualShock 2's motors through padSetActDirect if it is missed
})
-- Scripted presses, for tests: PAD="1500:a 1560:dpdown" in the environment (host) presses those buttons at
-- those frames; the bot of MODE "pad" calls press. A press is held for two frames (the d-pad repeats after 0.3 s).
local pressed_until = {}
local pad_seen = {}  -- the buttons pressed since the last PERF line, which lists them
local function press(name) pressed_until[name] = frame + 2 end
local scripted = {}
for at, name in (os.getenv("PAD") or ""):gmatch("(%d+):(%w+)") do
  scripted[tonumber(at)] = scripted[tonumber(at)] or {}
  table.insert(scripted[tonumber(at)], name)
end
local function pump()
  for _, name in ipairs(scripted[frame] or {}) do press(name) end
  local state = {pad.read()}
  local bits, moved = state[1], 0
  for n, name in ipairs(AXES) do
    axes[name] = state[n + 1]
    moved = math.max(moved, math.abs(state[n + 1]))
  end
  if moved > 0.2 then events[#events + 1] = {"joystickaxis", gamepad, 1, moved} end
  for n, name in ipairs(BUTTONS) do
    local down = math.floor(bits / 2 ^ (n - 1)) % 2 == 1 or (pressed_until[name] or 0) > frame
    if axes[name] then
      axes[name] = down and 1 or 0
      if down then events[#events + 1] = {"joystickaxis", gamepad, 5, 1} end
    elseif down ~= (held[name] or false) then
      held[name] = down
      if down then pad_seen[#pad_seen + 1] = name end
      events[#events + 1] = {down and "gamepadpressed" or "gamepadreleased", gamepad, name}
    end
  end
end

local quit
love.event = module("event", {
  pump = pump,
  poll = function()
    local list, n = events, 0
    events = {}
    return function()
      n = n + 1
      if list[n] then return unpack(list[n]) end
    end
  end,
  quit = function() quit = true end,
})
love.handlers = setmetatable({}, {__index = function(_, name)
  return function(...) if love[name] then return love[name](...) end end
end})

---------------------------------------------------------------- system, window, input
love.system = module("system", {
  getOS = function() return "ps4" end,  -- the game's own PlayStation branch (globals.lua)
  getClipboardText = function() return "" end,
  setClipboardText = nop,
  openURL = nop,
})

local W, H = 640, 448
love.window = module("window", {
  setMode = function() return true end,
  updateMode = function() return true end,
  getMode = function() return W, H, {fullscreen = true, vsync = 1, display = 1} end,
  getDesktopDimensions = function() return W, H end,
  getDisplayCount = function() return 1 end,
  getDisplayName = function() return "PlayStation 2" end,
  getFullscreenModes = function() return {{width = W, height = H}} end,
  getTitle = function() return "Balatro" end,
  isOpen = function() return true end,
  toPixels = function(x) return x end,
})

love.mouse = module("mouse", {
  getPosition = function() return 0, 0 end,
  isVisible = function() return false end,
  setVisible = nop, setGrabbed = nop, setRelativeMode = nop,
})
love.touch = module("touch", {getTouches = function() return {} end})
love.joystick = module("joystick", {
  getJoysticks = function() return {gamepad} end,
  loadGamepadMappings = nop,
})

---------------------------------------------------------------- graphics: gfx.c draws, this maps LÖVE's objects
gfx.init(W, H, os.getenv("SHOT"))
local info = setmetatable({}, {__mode = "k"})  -- per object: a texture {id, w, h, k = texels per unit}, a quad {x, y, w, h}
gfx.info(info)  -- gfx.c draws a Text from here: {text = string, k = size factor}
local function sized(kind, id, w, h, k)
  local o = object(kind, {
    getWidth = function() return w end,
    getHeight = function() return h end,
    getDimensions = function() return w, h end,
    getPixelWidth = function() return w end,
    getPixelHeight = function() return h end,
    setFilter = nop,
  })
  info[o] = {id = id, w = w, h = h, k = k}
  return o
end
-- Always the 1x atlas: the game asks for 2x by default, but with its dpiscale it works in 1x units either way
-- (game.lua:1100); the few 2x images it loads with dpiscale 1 are told to be twice their size.
local function new_image(path, opts)
  local twice = path:find("/2x/", 1, true) and not (opts and opts.dpiscale == 2)
  local id, w, h = gfx.texture(GAME .. "/" .. (path:gsub("/2x/", "/1x/")) .. ".tex")
  if twice then return sized("Image", id, w * 2, h * 2, 0.5) end
  return sized("Image", id, w, h, 1)
end
-- The bitmap font of tools/font.py (design D8): the glyphs at their native 16 px, the advances LÖVE measures at
-- the size the game asks for. ponytail: this one font whatever file is asked for; the game's other eight are
-- Noto for the languages the port does not carry.
local FONT = assert(loadstring(assert(read_game("resources/fonts/m6x11plus.ttf.lua"), "no font: run tools/font.py")))()
gfx.font(gfx.texture(GAME .. "/resources/fonts/m6x11plus.ttf.tex"), FONT.size / FONT.native)
for cp, g in pairs(FONT.glyphs) do gfx.glyph(cp, unpack(g)) end
local current_font
local function new_font(file, size)
  if type(file) == "number" then size = file end
  local k = (size or 12) / FONT.size
  local f = object("Font", {
    getWidth = function(_, text) return gfx.width(tostring(text)) * k end,
    getHeight = function() return math.floor(FONT.height * k + 0.5) end,
    hasGlyphs = function(_, text) return select(2, gfx.width(text)) end,
    setFilter = nop,
  })
  info[f] = {k = k}
  return f
end
local function draw_text(font, text, ...) gfx.text(text, info[font].k, ...) end
-- ponytail: the colours of a coloured text ({colour, string, ...}) are dropped; the game only passes white
local function plain(text)
  if type(text) ~= "table" then return tostring(text or "") end
  local parts = {}
  for _, part in ipairs(text) do if type(part) ~= "table" then parts[#parts + 1] = tostring(part) end end
  return table.concat(parts)
end
local function draw(drawable, a, ...)
  local t = info[drawable]
  if t.text then return gfx.text(t.text, t.k, a, ...) end
  local q, k = info[a], t.k
  if q then gfx.blit(t.id, q[1] * k, q[2] * k, q[3] * k, q[4] * k, q[3], q[4], ...)
  else gfx.blit(t.id, 0, 0, t.w * k, t.h * k, t.w, t.h, a, ...) end
end

love.graphics = module("graphics", {
  isActive = function() return true end,
  isCreated = function() return true end,
  getWidth = function() return W end,
  getHeight = function() return H end,
  getDimensions = function() return W, H end,
  newImage = new_image,
  newCanvas = function(w, h) w, h = w or W, h or H return sized("Canvas", gfx.canvas(w, h), w, h, 1) end,
  newQuad = function(x, y, w, h)
    local q = {x, y, w, h}
    local o = object("Quad", {setViewport = function(_, x, y, w, h) q[1], q[2], q[3], q[4] = x, y, w, h end})
    info[o] = q
    return o
  end,
  -- a shader is its file's name and what was sent to it: gfx.c has a recipe for some names
  newShader = function(path)
    local uniforms = {}
    local o = object("Shader", {send = function(_, name, value) uniforms[name] = value end})
    info[o] = {name = path:match("([%w_]+)%.fs$"), uniforms = uniforms}
    return o
  end,
  newFont = new_font,
  setNewFont = function(...) current_font = new_font(...) return current_font end,
  newText = function(font, text)
    local t = {text = plain(text), k = info[font].k}
    local o
    o = object("Text", {
      set = function(_, s) t.text = plain(s) end,
      getWidth = function() return font:getWidth(t.text) end,
      getHeight = function() return font:getHeight() end,
      getDimensions = function() return o:getWidth(), o:getHeight() end,
    })
    info[o] = t
    return o
  end,
  -- ponytail: printf does not wrap or align (the crash screen's text, main.lua:290)
  print = function(text, x, y, r, sx, sy) draw_text(current_font, tostring(text), x, y, r, sx, sy) end,
  printf = function(text, font, x, y)
    if type(font) == "number" then font, x, y = current_font, font, x end
    draw_text(font, plain(text), x, y)
  end,
  draw = draw,
  setCanvas = function(c)
    if type(c) == "table" then c = c[1] end
    gfx.target(c and info[c].id)
  end,
  reset = function() gfx.origin() gfx.setColor(1, 1, 1, 1) gfx.target() end,
  clear = gfx.clear, origin = gfx.origin, pop = gfx.pop, push = gfx.push, polygon = gfx.polygon,
  rectangle = gfx.rectangle, rotate = gfx.rotate, scale = gfx.scale, translate = gfx.translate,
  setColor = gfx.setColor, setLineWidth = gfx.setLineWidth,
  present = gfx.present,
  setDefaultFilter = nop, setLineStyle = nop,
  setShader = function(s)
    if s then gfx.shader(info[s].name, info[s].uniforms) else gfx.shader() end
  end,
})

---------------------------------------------------------------- audio: snd.c mixes, this maps LÖVE's sources
-- SND_LOG=1 in the environment (host), or MODE "pad": a line for every source that starts. PERF lines name the
-- music layer that sounds loudest.
local whole = {}  -- path -> the sample of a sound snd.c keeps whole in memory, shared by its sources
local sources = setmetatable({}, {__mode = "k"})
local SND_LOG = os.getenv("SND_LOG") or MODE == "pad"
local layers = {}  -- music layer -> its volume
local function loudest()
  local name, most = "none", 0
  for layer, volume in pairs(layers) do if volume > most then name, most = layer, volume end end
  return name
end
local function new_source(path, kind)
  local sample, shared = whole[path], true
  if not sample then
    sample, shared = snd.open(GAME .. "/" .. path .. ".snd", kind == "stream")
    if shared then whole[path] = sample end
  end
  local voice, volume, pitch = nil, 1, 1
  local layer = path:match("music%d")
  local o
  o = object("Source", {
    play = function()
      if voice and snd.playing(voice) then return true end
      voice = snd.start(sample, volume, pitch)
      if SND_LOG then print(("SND %s volume %.2f pitch %.2f"):format(path, volume, pitch)) end
      return voice ~= nil
    end,
    stop = function() if voice then snd.stop(voice) voice = nil end end,
    isPlaying = function() return voice ~= nil and snd.playing(voice) end,
    setVolume = function(_, v)
      volume = math.max(0, math.min(v, 1))  -- OpenAL's gain limits
      if layer then layers[layer] = volume end
      if voice then snd.set(voice, volume, pitch) end
    end,
    setPitch = function(_, v)
      pitch = v
      if voice then snd.set(voice, volume, pitch) end
    end,
    release = function()
      o:stop()
      if not shared and sample then snd.close(sample) sample = nil end
    end,
  })
  sources[o] = true
  return o
end
love.audio = module("audio", {
  newSource = new_source,
  play = function(source) return source:play() end,
  stop = function() for source in pairs(sources) do source:stop() end end,
})
love.sound = module("sound", {})

---------------------------------------------------------------- threads
-- The game's sound manager (engine/sound_manager.lua) is a LÖVE thread: a loop that waits for requests on a
-- channel. Here it is a coroutine with its own globals: demand on an empty channel yields, and a push resumes it,
-- so a request is served inside the push that makes it (design D10: no kernel thread needed, the mixer has its own).
-- A pushed table is copied, as between LÖVE's threads: the game reuses its request tables.
-- The save manager (engine/save_manager.lua) runs the same way: a save is packed, deflated and written inside
-- the push that asks for it. Its tables are not copied: they are used before the push returns.
-- ponytail: so a save stops the frame for as long as the card takes. An EE thread doing the write if it shows.
-- ponytail: the http thread does not run and its channel drops what is pushed: there is no network.
local function copy(v)
  if type(v) ~= "table" then return v end
  local t = {}
  for k, x in pairs(v) do t[k] = copy(x) end
  return t
end
local function resume(co, ...)
  local ok, err = coroutine.resume(co, ...)
  if not ok then error(debug.traceback(co, err), 0) end
end
local QUEUED = {sound_request = true, load_channel = true, save_request = true}
local channels = {}
love.thread = module("thread", {
  newThread = function(file)
    return object("Thread", {start = function(_, ...)
      if file ~= "engine/sound_manager.lua" and file ~= "engine/save_manager.lua" then return end
      local chunk = assert(love.filesystem.load(file))
      setfenv(chunk, setmetatable({}, {__index = _G}))
      resume(coroutine.create(chunk), ...)
    end})
  end,
  getChannel = function(name)
    if channels[name] then return channels[name] end
    local queue, first, last, waiting = {}, 1, 0, nil
    local function pop()
      if first > last then return nil end
      local v = queue[first]
      queue[first] = nil
      first = first + 1
      return v
    end
    channels[name] = object("Channel", {
      push = function(_, v)
        if not QUEUED[name] then return end
        last = last + 1
        queue[last] = name == "save_request" and v or copy(v)
        if waiting then
          local co, t, card = waiting, clock(), card_time
          waiting = nil
          resume(co)
          if CARD and name == "save_request" then
            print(("SAVE %s in %.0f ms, %.0f of them writing"):format(tostring(v.type), (clock() - t) * 1000, (card_time - card) * 1000))
          end
        end
      end,
      pop = pop,
      demand = function()
        while first > last do
          waiting = coroutine.running()
          coroutine.yield()
        end
        return pop()
      end,
    })
    return channels[name]
  end,
})

-- L�VE's "deflate" is the raw format; the game only asks for strings (string_packer.lua)
love.data = module("data", {
  compress = function(container, format, data, level)
    assert(container == "string" and format == "deflate", "love.data.compress: only deflate to a string")
    return save.deflate(data, level)
  end,
  decompress = function(container, format, data)
    assert(container == "string" and format == "deflate", "love.data.decompress: only deflate to a string")
    return save.inflate(data)
  end,
})

current_font = new_font(12)
for name, mod in pairs(love) do package.loaded["love." .. name] = mod end

---------------------------------------------------------------- boot, as LÖVE's boot.lua: conf, main, love.run
-- TRACE=<n> in the environment (host): a deterministic run (below) and, every n frames,
-- a line that sums every moveable's transform, to compare two builds of the runtime (runtime/test/native.sh).
local TRACE = tonumber(os.getenv("TRACE"))
if TRACE then
  os.time = function() return 1700000000 end
  -- and a clock that only counts its calls: the game's GC budget (nuGC) then takes the same steps in every run,
  -- so objects get the same addresses and tables keyed by objects iterate in the same order
  local calls = 0
  love.timer.getTime = function() calls = calls + 1 return calls * 1e-4 end
end
local t0 = clock()
-- watchdog: a step (the load, or one frame) that runs for WATCHDOG seconds aborts with its traceback
local WATCHDOG, step_start = tonumber(os.getenv("WATCHDOG") or 120), t0
-- PROFILE=<n> in the environment (host): sample the running function every n VM instructions, per game state
-- (neither on the EE: a count hook costs every VM instruction a decrement)
local PROFILE, samples, ticks = tonumber(os.getenv("PROFILE")), {}, 0
if not sys.ee then debug.sethook(function()
  ticks = ticks + 1
  if PROFILE then
    local info = debug.getinfo(2, "S")
    local state = G and G.STATE or 0
    local key = info.short_src .. ":" .. info.linedefined
    samples[state] = samples[state] or {}
    samples[state][key] = (samples[state][key] or 0) + 1
  end
  if ticks % (PROFILE and 10000 or 1) == 0 and clock() - step_start > WATCHDOG then
    error("watchdog: stuck for " .. WATCHDOG .. " s", 2)
  end
end, "", PROFILE or 10000000) end

-- ponytail: the game stops Lua's collector and steps it for 0.3 ms a frame (nuGC), with a 300 MB safety net. On
-- the EE that budget collects too little and the heap grew past 13 MB of the 32. The runtime keeps the
-- incremental collector running (the game's "stop" does nothing), paced by allocation. GC=game in the
-- environment (host) leaves the game's own scheme, to compare.
if os.getenv("GC") ~= "game" then
  local gc = collectgarbage
  -- a new cycle as soon as one ends, and ten times the work of Lua's default for each byte allocated: the heap of a
  -- bot run peaks a fifth lower than with 130 / 300 (docs/perf.md), and the game allocates little per frame
  gc("setpause", tonumber(os.getenv("GC_PAUSE") or 100))
  gc("setstepmul", tonumber(os.getenv("GC_STEPMUL") or 1000))
  function collectgarbage(opt, arg)
    if opt == "stop" then return 0 end
    return gc(opt, arg)
  end
end
assert(love.filesystem.load("conf.lua"))()
love.conf({window = {}, modules = {}})
require("main")

-- The game's hot engine functions in C (engine.c). NATIVE=0 in the environment (host) leaves the Lua ones.
-- The C ones call each other directly, so no class may override them (move itself may: it is called by name).
if engine and os.getenv("NATIVE") ~= "0" then
  for _, name in ipairs{"move_juice", "move_xy", "move_scale", "move_wh", "move_r", "calculate_parrallax",
                        "lr_clamp", "align_to_major", "get_major", "glue_to_major", "move_with_major"} do
    for cname, cls in pairs(_G) do
      if type(cls) == "table" and cls ~= Moveable and rawget(cls, "__index") == cls and rawget(cls, name) then
        error(("engine.c assumes only Moveable defines %s, but %s does too"):format(name, cname))
      end
    end
  end
  engine.install()
end
local t_main = clock()

local spent = {update = 0, draw = 0}
local worst = {update = 0, draw = 0}
local total = {update = 0, draw = 0}
for name in pairs(spent) do
  local fn = love[name]
  love[name] = function(...)
    local t = clock()
    fn(...)
    t = clock() - t
    spent[name] = spent[name] + t
    total[name] = total[name] + t
    if frame > 1 and t > worst[name] then worst[name] = t end
  end
end

local loop = love.run()
local t_load = clock()
print(("BOOT number=%d bytes  main.lua %.2f s  love.load %.2f s  heap %d KB  malloc %d KB"):format(
  NUMBER_SIZE, t_main - t0, t_load - t_main, collectgarbage("count"), sys.mem() / 1024))
spent.update, spent.draw = 0, 0
-- sys.sections (-DSECTIONS), or SECTIONS=1 in the environment (host): the game's own checkpoints of a frame
-- (timer_checkpoint, which does nothing without its overlay) timed, a SECT line with every PERF line
-- sys.prof_section (-DPROF -DPROF_SECTION=n): the profiler samples only between two of those checkpoints
local sections, section_at
local gate = ({{"animate", "move"}, {"move", "update"}, {"start->canvas", "primatives"}, {"primatives", "uiboxes"}, {"uiboxes", "rest"}, {"update", "tail"}})[sys.prof_section]
if sys.sections or os.getenv("SECTIONS") or gate then
  sections = {}
  if gate then sys.prof_gate(false) end
  function timer_checkpoint(label, kind, reset)
    local now = clock()
    if reset and kind == "draw" and section_at then -- what an update does after its last checkpoint (the controller)
      sections["update tail"] = (sections["update tail"] or 0) + now - section_at
      if gate and gate[2] == "tail" then sys.prof_gate(false) end
    elseif not reset then
      local key = kind .. " " .. label
      sections[key] = (sections[key] or 0) + now - section_at
      if gate and label == gate[1] then sys.prof_gate(true) elseif gate and label == gate[2] then sys.prof_gate(false) end
    end
    section_at = now
  end
end

-- MODE "run": the bench plays, through the game's own functions (what its buttons call): from the menu a
-- seeded run with the tutorial off, then the small blind, then the first five cards of every hand.
local state, state_frames = nil, 0
local function drive()
  if G.STATE ~= state then state, state_frames = G.STATE, 0 end
  state_frames = state_frames + 1
  if state_frames < 120 then return end  -- two seconds into each state, once what it acts on exists
  if state == G.STATES.MENU then
    G.SETTINGS.tutorial_complete = true
    G.FUNCS.start_run(nil, {stake = 1, seed = "BENCH"})
    state_frames = -100000  -- once
  elseif state == G.STATES.BLIND_SELECT and G.blind_select and G.blind_select.UIRoot then
    G.FUNCS.select_blind(G.blind_select.UIRoot.children[1].children[1].config.object:get_UIE_by_ID("select_blind_button"))
    state_frames = -100000
  elseif state == G.STATES.SELECTING_HAND and G.hand and #G.hand.cards >= 5 and not G.play.cards[1] then
    for i = 1, 5 do G.hand:add_to_highlighted(G.hand.cards[i]) end
    G.FUNCS.play_cards_from_highlighted()
    state_frames = 0  -- the next hand comes back to this same state
  end
end

-- DRESS=<frame> in the environment (host; -DDRESS=<frame> on the EE): from that frame of a run the cards in hand put on what the card shaders
-- draw (an edition each, a debuff, the grey of a card that cannot be played, a gold seal) and a negative Joker,
-- Hologram, a voucher and a booster pack join the Jokers, for a screenshot of every recipe of gfx.c.
local DRESS, BLIND = tonumber(os.getenv("DRESS")) or sys.dress, tonumber(os.getenv("BLIND")) or sys.blind
local function dress()
  local hand = G.hand.cards
  hand[1]:set_edition({foil = true}, true, true)
  hand[2]:set_edition({holo = true}, true, true)
  hand[3]:set_edition({polychrome = true}, true, true)
  hand[4]:set_edition({negative = true}, true, true)
  hand[5]:set_debuff(true)
  hand[6].greyed = true
  hand[7]:set_seal("Gold", true, true)
  for _, key in ipairs{"j_joker", "j_hologram", "v_overstock_norm", "p_arcana_normal_1"} do
    local card = Card(G.jokers.T.x, G.jokers.T.y, G.CARD_W, G.CARD_H, nil, G.P_CENTERS[key], {bypass_discovery_center = true})
    if key == "j_joker" then card:set_edition({negative = true}, true, true) end
    G.jokers:emplace(card)
  end
  -- BLIND=<chips> (-DBLIND): the blind asks for that few, so the first hand beats it and the flames of flame.fs light
  if BLIND then G.GAME.blind.chips = BLIND end
end

local function trace(i)
  local s, n = 0, 0
  for j, m in ipairs(G.MOVEABLES) do
    local T, VT, v = m.T, m.VT, m.velocity
    s = s + j * (T.x + 2 * T.y + 3 * T.w + 5 * T.h + 7 * T.r + 11 * VT.x + 13 * VT.y + 17 * VT.w + 19 * VT.h +
                 23 * VT.r + 29 * VT.scale + 31 * v.x + 37 * v.y + 41 * v.r + 43 * v.scale)
    n = j
  end
  print(("TRACE %d state %s moveables %d sum %.17g chips %s"):format(i, tostring(G.STATE), n, s,
    tostring(G.GAME and G.GAME.chips)))
end

-- SHOT=<frame>,<frame>... in the environment (host): those frames of the screen as shot-<frame>.ppm.
-- On the EE the frames come from -DSHOTS='"900,1600"': each stays on screen for three seconds after a SHOT line,
-- for tools/pcsx2.sh to press the emulator's screenshot key.
local SHOT = {}
for n in (os.getenv("SHOT") or SHOTS):gmatch("%d+") do SHOT[tonumber(n)] = true end
local function shot(i)
  if os.getenv("SHOT") then return gfx.shot(("shot-%d.ppm"):format(i)) end
  print("SHOT " .. i)
  local t = clock()
  while clock() - t < 3 do end
end

-- MODE "pad": a bot plays a new run up to beating Ante 1, buying a Joker in the first shop that sells one it can
-- pay, with pad presses alone (press): the d-pad moves the game's focus, cross clicks, square / triangle / R1 are
-- the game's own shortcuts. It reads the game's state to decide, never writes it, but for one setting: the
-- tutorial is off. GATE lines tell how it went.
local bot_next, bot_tries, bot_from, bot_plan, bot_plan_key, bot_shots = 0, 0, nil, nil, nil, {}
local function milestone(name)
  if bot_shots[name] then return end
  bot_shots[name] = true
  print("BOT " .. name .. " at frame " .. frame)
  if os.getenv("SHOT") or sys.ee then shot(frame) end
end
-- true once the game's focus is on node; else one d-pad press towards it
local function focus_on(node)
  local f = G.CONTROLLER.focused.target
  if f == node then return true end
  if not f then return press("dpdown") end
  if f.area and f.area == node.area then return press(f.rank < node.rank and "dpright" or "dpleft") end
  local fx, fy = f:put_focused_cursor()
  local nx, ny = node:put_focused_cursor()
  local dx, dy = nx - fx, ny - fy
  local horizontal = math.abs(dx) > math.abs(dy)
  bot_tries = f == bot_from and bot_tries + 1 or 0  -- a press that did not move the focus: the other axis next
  bot_from = f
  if bot_tries % 2 == 1 then horizontal = not horizontal end
  press(horizontal and (dx > 0 and "dpright" or "dpleft") or (dy > 0 and "dpdown" or "dpup"))
end
local function click(node) if node and focus_on(node) then press("a") return true end end  -- nil: already gone
-- the cards to select and the button for them: the best five by the game's own hand evaluation, or, when that
-- would not reach the blind at this pace and discards are left, the cards outside it to discard
local function plan_hand()
  local cards, best, best_score, best_scoring = G.hand.cards, nil, -1, nil
  local set = {}
  local function choose(from, left)
    if left == 0 or from > #cards then
      if #set == 0 then return end
      local text, _, _, scoring = G.FUNCS.get_poker_hand_info(set)
      local chips = G.GAME.hands[text].chips
      for _, c in ipairs(scoring) do chips = chips + c:get_chip_bonus() end
      local score = chips * G.GAME.hands[text].mult
      if score > best_score then
        best, best_score, best_scoring = {unpack(set)}, score, scoring
      end
      return
    end
    for i = from, #cards do
      set[#set + 1] = cards[i]
      choose(i + 1, left - 1)
      set[#set] = nil
    end
  end
  choose(1, math.min(5, #cards))
  local round = G.GAME.current_round
  local need = (G.GAME.blind.chips - G.GAME.chips) / math.max(round.hands_left, 1)
  if best_score < need and round.discards_left > 0 then
    local keep, out = {}, {}
    for _, c in ipairs(best_scoring) do keep[c] = true end
    for _, c in ipairs(cards) do if not keep[c] and #out < 5 then out[#out + 1] = c end end
    if #out > 0 then return {cards = out, button = "y", what = ("discard %d (best %d, need %d)"):format(#out, best_score, need)} end
  end
  return {cards = best, button = "x", what = ("play for about %d (need %d)"):format(best_score, need)}
end
-- MODE "saves", the gate of phase 5, is the same bot over two boots. With no run on the card it sets the game
-- speed to 2 (a setting to find again) and plays until the game has saved the run with a hand played: the power
-- off. With a run on the card it continues it from the menu and compares what the game then holds with what the
-- file held.
local resumed  -- the run found on the card, unpacked; false for none
local seen_save  -- the last contents of the run's file looked at
local function saved_run()
  local s = get_compressed("1/save.jkr")
  return s and STR_UNPACK(s)
end
local function describe(game, hand)
  return ("round %d  chips %s  hands left %d  dollars %s  cards in hand %d"):format(
    game.round, tostring(game.chips), game.current_round.hands_left, tostring(game.dollars), hand)
end
local function bot()
  if not (G and G.SETTINGS and G.STATES and G.CONTROLLER) then return end
  if MODE == "saves" then
    if resumed == nil then
      resumed = saved_run() or false
      print("SAVES " .. (resumed and "a run is on the card: " .. describe(resumed.GAME, #resumed.cardAreas.hand.cards) or "no run on the card"))
    end
    local round = G.GAME and G.GAME.current_round
    if resumed and G.STAGE == G.STAGES.RUN and G.hand and G.GAME.current_round then
      local now, was = describe(G.GAME, #G.hand.cards), describe(resumed.GAME, #resumed.cardAreas.hand.cards)
      print("SAVES continued: " .. now)
      print((now == was and G.SETTINGS.GAMESPEED == 2 and love.filesystem.getInfo("1/profile.jkr"))
        and "GATE PASSED: the settings, the profile and the run in progress came back from the card"
        or ("GATE FAILED: the card held '%s', game speed %s"):format(was, tostring(G.SETTINGS.GAMESPEED)))
      love.filesystem.remove("1/save.jkr")  -- so the next run of the gate starts from its first half
      quit = true
      return
    elseif not resumed and G.STATE == G.STATES.MENU and G.SETTINGS.GAMESPEED ~= 2 then
      G.SETTINGS.GAMESPEED = 2
      G:save_settings()
    elseif not resumed and round and round.hands_played >= 1 and saves["1/save.jkr"] ~= seen_save then
      seen_save = saves["1/save.jkr"]
      local run = saved_run()  -- the game writes it within its save timer
      if run and run.GAME.current_round.hands_played >= 1 then
        print("SAVES saved: " .. describe(run.GAME, #run.cardAreas.hand.cards))
        print("GATE HALF: power off, and boot again to continue this run")
        quit = true
        return
      end  -- else it plays on: the game saves again after the next hand
    end
  end
  G.SETTINGS.tutorial_complete = true
  if G.TIMERS.REAL < bot_next or G.CONTROLLER.locked then return end
  bot_next = G.TIMERS.REAL + 0.4
  local state, S = G.STATE, G.STATES
  if G.GAME and G.GAME.round_resets and G.GAME.round_resets.ante >= 2 and state == S.BLIND_SELECT then
    milestone("ante 2")
    print(("GATE PASSED: Ante 1 beaten with the pad alone in %d rounds; Jokers bought: %d"):format(G.GAME.round, #G.jokers.cards))
    quit = true
    return
  end
  if state == S.MENU then
    if G.OVERLAY_MENU then press("x")  -- the run setup: the red deck is the first one; square is its Play
    elseif G.MAIN_MENU_UI then click(G.MAIN_MENU_UI:get_UIE_by_ID("main_menu_play")) end
  elseif state == S.BLIND_SELECT and G.blind_select_opts then
    local box = G.blind_select_opts[G.GAME.blind_on_deck:lower()]
    if box and click(box:get_UIE_by_ID("select_blind_button")) then print("BOT blind " .. G.GAME.blind_on_deck) end
  elseif state == S.SELECTING_HAND and G.hand and #G.hand.cards > 0 then
    local round = G.GAME.current_round
    local key = ("%d %d %d %d"):format(G.GAME.round, round.hands_left, round.discards_left, #G.hand.cards)
    if key ~= bot_plan_key then bot_plan, bot_plan_key = plan_hand(), key end
    local want, lit = {}, {}
    for _, c in ipairs(bot_plan.cards) do want[c] = true end
    for _, c in ipairs(G.hand.highlighted) do
      lit[c] = true
      if not want[c] then return click(c) end
    end
    for _, c in ipairs(bot_plan.cards) do if not lit[c] then return click(c) end end
    milestone("first hand selected")
    print("BOT " .. bot_plan.what)
    press(bot_plan.button)
    bot_next = G.TIMERS.REAL + 2
  elseif state == S.ROUND_EVAL then  -- the game puts the focus on Cash Out when that button appears
    local f = G.CONTROLLER.focused.target
    if f and f.config.button == "cash_out" then press("a") print("BOT cash out, round " .. G.GAME.round) end
  elseif state == S.SHOP and G.shop_jokers and G.jokers then
    milestone("shop")
    if #G.jokers.cards == 0 then
      for _, c in ipairs(G.shop_jokers.cards) do
        if c.ability.set == "Joker" and c.cost <= G.GAME.dollars then
          if focus_on(c) then press("rightshoulder") print("BOT buy " .. c.ability.name) end
          return
        end
      end
    else
      milestone("bought " .. G.jokers.cards[1].ability.name)
    end
    press("y")  -- next round
    bot_next = G.TIMERS.REAL + 2
  elseif state == S.GAME_OVER then
    print(("GATE FAILED: game over in ante %d, round %d"):format(G.GAME.round_resets.ante, G.GAME.round))
    quit = true
  end
end

-- sys.osd (-DOSD), or OSD=1 in the environment (host): a line over the picture with the numbers of the last 60
-- frames, the PERF line's: frames a second by the clock, the milliseconds of update and of draw, the Lua heap and
-- everything allocated.
local osd, osd_from = "", clock()
if sys.osd or os.getenv("OSD") then
  local present = love.graphics.present
  love.graphics.present = function()
    gfx.push()
    gfx.origin()
    gfx.setColor(0, 0, 0, 0.7)
    gfx.rectangle("fill", 20, 20, W - 40, 16)  -- clear of a television's overscan
    gfx.setColor(1, 1, 1, 1)
    gfx.text(osd, 14 / FONT.size, 26, 21)
    gfx.pop()
    present()
  end
end

local peak, allocated = 0, sys.allocated()
for i = 1, FRAMES > 0 and FRAMES or math.huge do
  frame = i
  step_start = clock()
  if loop() or quit then break end
  local playing = snd.pump(dt)
  if MODE == "run" then drive() end
  if BOT then bot() end
  if sys.prof and i == 1300 then sys.prof(true) end
  if TRACE and i % TRACE == 0 then trace(i) end
  if DRESS and i >= DRESS and G.hand and #G.hand.cards >= 7 then DRESS = dress() end  -- (once)
  if SHOT[i] then shot(i) end
  peak = math.max(peak, collectgarbage("count"))
  if i % 60 == 0 then  -- averages and worst frames of the last 60
    print(("PERF frame %d  update %.2f ms (worst %.1f)  draw %.2f ms (worst %.1f)  heap %d KB (peak %d)  malloc %d KB  alloc %.1f KB/frame  prims %d  state %s  voices %d  music %s  pad %s"):format(
      i, spent.update / 60 * 1000, worst.update * 1000, spent.draw / 60 * 1000, worst.draw * 1000,
      collectgarbage("count"), peak, sys.mem() / 1024, (sys.allocated() - allocated) / 60, gfx.count() / 60, tostring(G and G.STATE), playing, loudest(), table.concat(pad_seen, ",")))
    if sections then
      local out = {}
      for key, t in pairs(sections) do out[#out + 1] = ("%s %.1f"):format(key, t / 60 * 1000) end
      table.sort(out)
      print("SECT ms: " .. table.concat(out, " | "))
      sections = {}
    end
    osd = ("%.1f fps  update %d ms  draw %d ms  heap %.1f MB  all %.1f MB"):format(
      60 / (clock() - osd_from), spent.update / 60 * 1000, spent.draw / 60 * 1000, collectgarbage("count") / 1024, sys.mem() / 1048576)
    osd_from = clock()
    pad_seen = {}
    spent.update, spent.draw, worst.update, worst.draw = 0, 0, 0, 0
    allocated = sys.allocated()
  end
end
if sys.prof then sys.prof(false) end
print(("TOTAL %d frames  update %.2f s  draw %.2f s  %s"):format(frame, total.update, total.draw, jit and jit.version or _VERSION))

for state, counts in pairs(samples) do
  local list, total = {}, 0
  for key, n in pairs(counts) do list[#list + 1] = {key, n} total = total + n end
  table.sort(list, function(a, b) return a[2] > b[2] end)
  print(("PROFILE state %s: %d samples"):format(tostring(state), total))
  for i = 1, math.min(25, #list) do
    print(("  %5.1f %%  %s"):format(list[i][2] / total * 100, list[i][1]))
  end
end
