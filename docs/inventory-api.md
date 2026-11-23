# LÖVE API inventory

47 Lua files, 98407 lines (comments stripped).

## love.* (106 names)

| Name | Uses | Files |
|---|---|---|
| love.arg.parseGameArguments | 1 | main.lua |
| love.audio | 2 | engine/sound_manager.lua, main.lua |
| love.audio.newSource | 3 | engine/sound_manager.lua, functions/misc_functions.lua |
| love.audio.play | 4 | engine/sound_manager.lua, functions/misc_functions.lua |
| love.audio.stop | 1 | main.lua |
| love.conf | 1 | conf.lua |
| love.data.compress | 1 | engine/string_packer.lua |
| love.data.decompress | 1 | engine/string_packer.lua |
| love.draw | 3 | main.lua |
| love.errhand | 1 | main.lua |
| love.event | 2 | main.lua |
| love.event.poll | 2 | main.lua |
| love.event.pump | 2 | main.lua |
| love.event.quit | 2 | functions/button_callbacks.lua, main.lua |
| love.filesystem | 1 | engine/save_manager.lua |
| love.filesystem.append | 3 | engine/save_manager.lua, game.lua |
| love.filesystem.createDirectory | 4 | engine/save_manager.lua, game.lua |
| love.filesystem.getDirectoryItems | 3 | engine/sound_manager.lua, game.lua |
| love.filesystem.getInfo | 17 | engine/save_manager.lua, engine/string_packer.lua, functions/button_callbacks.lua, functions/misc_functions.lua, game.lua |
| love.filesystem.getSourceBaseDirectory | 1 | main.lua |
| love.filesystem.read | 3 | engine/string_packer.lua, game.lua |
| love.filesystem.remove | 15 | functions/button_callbacks.lua, functions/common_events.lua, functions/misc_functions.lua, game.lua |
| love.filesystem.write | 1 | engine/string_packer.lua |
| love.gamepadpressed | 2 | main.lua |
| love.gamepadreleased | 2 | main.lua |
| love.graphics | 2 | main.lua |
| love.graphics.clear | 5 | functions/misc_functions.lua, game.lua, main.lua |
| love.graphics.dis | 1 | functions/misc_functions.lua |
| love.graphics.draw | 10 | engine/animatedsprite.lua, engine/sprite.lua, engine/text.lua, engine/ui.lua, game.lua |
| love.graphics.getHeight | 5 | functions/button_callbacks.lua, game.lua |
| love.graphics.getWidth | 6 | functions/button_callbacks.lua, game.lua, main.lua |
| love.graphics.isActive | 1 | main.lua |
| love.graphics.isCreated | 1 | main.lua |
| love.graphics.newCanvas | 2 | main.lua |
| love.graphics.newFont | 1 | game.lua |
| love.graphics.newImage | 3 | game.lua |
| love.graphics.newQuad | 2 | engine/animatedsprite.lua, engine/sprite.lua |
| love.graphics.newShader | 1 | game.lua |
| love.graphics.newText | 2 | engine/text.lua, engine/ui.lua |
| love.graphics.newVideo | 2 | functions/UI_definitions.lua, functions/test_functions.lua |
| love.graphics.origin | 1 | main.lua |
| love.graphics.polygon | 3 | engine/ui.lua |
| love.graphics.pop | 43 | engine/animatedsprite.lua, engine/node.lua, engine/particles.lua, engine/sprite.lua, engine/text.lua, engine/ui.lua, functions/misc_functions.lua, game.lua, main.lua |
| love.graphics.present | 3 | functions/misc_functions.lua, main.lua |
| love.graphics.print | 4 | engine/node.lua, functions/misc_functions.lua, game.lua |
| love.graphics.printf | 1 | main.lua |
| love.graphics.push | 30 | engine/node.lua, engine/particles.lua, functions/misc_functions.lua, game.lua, main.lua |
| love.graphics.rectangle | 9 | engine/node.lua, engine/particles.lua, engine/ui.lua, functions/misc_functions.lua, game.lua |
| love.graphics.reset | 1 | main.lua |
| love.graphics.rotate | 6 | engine/node.lua, engine/particles.lua, engine/ui.lua, functions/misc_functions.lua |
| love.graphics.scale | 17 | engine/animatedsprite.lua, engine/node.lua, engine/sprite.lua, engine/ui.lua, functions/misc_functions.lua, game.lua |
| love.graphics.setCanvas | 4 | functions/misc_functions.lua, game.lua |
| love.graphics.setColor | 40 | engine/animatedsprite.lua, engine/node.lua, engine/particles.lua, engine/sprite.lua, engine/text.lua, engine/ui.lua, functions/misc_functions.lua, game.lua, main.lua |
| love.graphics.setDefaultFilter | 1 | game.lua |
| love.graphics.setLineStyle | 1 | game.lua |
| love.graphics.setLineWidth | 7 | engine/node.lua, engine/ui.lua, functions/misc_functions.lua |
| love.graphics.setNewFont | 2 | functions/misc_functions.lua, main.lua |
| love.graphics.setShader | 8 | card.lua, engine/sprite.lua, functions/misc_functions.lua, game.lua |
| love.graphics.translate | 17 | engine/node.lua, engine/particles.lua, engine/text.lua, engine/ui.lua, functions/misc_functions.lua, game.lua |
| love.handlers | 2 | main.lua |
| love.joystick | 1 | main.lua |
| love.joystick.getJoysticks | 2 | game.lua, main.lua |
| love.joystick.loadGamepadMappings | 1 | game.lua |
| love.joystickaxis | 1 | main.lua |
| love.keypressed | 1 | main.lua |
| love.keyreleased | 1 | main.lua |
| love.load | 3 | main.lua |
| love.mouse | 1 | main.lua |
| love.mouse.getPosition | 1 | engine/controller.lua |
| love.mouse.isVisible | 1 | engine/controller.lua |
| love.mouse.setGrabbed | 1 | main.lua |
| love.mouse.setRelativeMode | 1 | main.lua |
| love.mouse.setVisible | 5 | engine/controller.lua, main.lua |
| love.mousemoved | 1 | main.lua |
| love.mousepressed | 1 | main.lua |
| love.mousereleased | 1 | main.lua |
| love.quit | 3 | main.lua |
| love.resize | 3 | functions/button_callbacks.lua, game.lua, main.lua |
| love.run | 1 | main.lua |
| love.sound | 1 | engine/sound_manager.lua |
| love.system | 3 | engine/http_manager.lua, engine/save_manager.lua, engine/sound_manager.lua |
| love.system.getClipboardText | 1 | functions/button_callbacks.lua |
| love.system.getOS | 14 | engine/http_manager.lua, engine/save_manager.lua, engine/sound_manager.lua, functions/button_callbacks.lua, game.lua, globals.lua, main.lua |
| love.system.openURL | 8 | functions/button_callbacks.lua |
| love.system.setClipboardText | 1 | functions/button_callbacks.lua |
| love.thread | 1 | engine/save_manager.lua |
| love.thread.getChannel | 12 | engine/http_manager.lua, engine/save_manager.lua, engine/sound_manager.lua, game.lua, main.lua |
| love.thread.newThread | 4 | game.lua, main.lua |
| love.timer | 4 | engine/save_manager.lua, main.lua |
| love.timer.getFPS | 1 | game.lua |
| love.timer.getTime | 8 | functions/misc_functions.lua, main.lua |
| love.timer.sleep | 3 | game.lua, main.lua |
| love.timer.step | 2 | main.lua |
| love.touch.getTouches | 1 | main.lua |
| love.update | 3 | main.lua |
| love.window | 1 | main.lua |
| love.window.getDesktopDimensions | 2 | functions/misc_functions.lua, main.lua |
| love.window.getDisplayCount | 1 | functions/misc_functions.lua |
| love.window.getFullscreenModes | 3 | functions/misc_functions.lua, main.lua |
| love.window.getMode | 2 | functions/misc_functions.lua |
| love.window.getTitle | 1 | main.lua |
| love.window.isOpen | 1 | main.lua |
| love.window.setMode | 1 | main.lua |
| love.window.showMessageBox | 1 | main.lua |
| love.window.toPixels | 1 | main.lua |
| love.window.updateMode | 1 | functions/button_callbacks.lua |

## Methods not defined by the game (36)

| Method | Uses |
|---|---|
| :demand | 4 |
| :exit_overlay_menu | 1 |
| :find | 13 |
| :getDimensions | 3 |
| :getGamepadAxis | 6 |
| :getGamepadMappingString | 1 |
| :getHeight | 3 |
| :getPixelHeight | 1 |
| :getSource | 2 |
| :getWidth | 3 |
| :gmatch | 2 |
| :gsub | 14 |
| :hasGlyphs | 1 |
| :isGamepad | 1 |
| :isPlaying | 11 |
| :len | 5 |
| :match | 2 |
| :options | 1 |
| :pause | 4 |
| :play | 4 |
| :pop | 2 |
| :push | 16 |
| :rep | 1 |
| :reverse | 2 |
| :seek | 2 |
| :send | 25 |
| :set | 1 |
| :setFilter | 2 |
| :setPitch | 4 |
| :setVibration | 2 |
| :setViewport | 1 |
| :setVolume | 7 |
| :shutdown | 1 |
| :start | 4 |
| :stop | 8 |
| :sub | 8 |

## LuaJIT / runtime risks

- **bit.***: `bit.bxor` x1, `bit.lshift` x1, `bit.rshift` x1
- **goto**: none
- **ffi**: none
- **jit.***: `jit.arch` x8, `jit.flush` x1, `jit.off` x5
- **require**: `require "back"` x1, `require "bit"` x1, `require "blind"` x1, `require "card"` x1, `require "card_character"` x1, `require "cardarea"` x1, `require "challenges"` x1, `require "engine/animatedsprite"` x1, `require "engine/controller"` x1, `require "engine/event"` x1, `require "engine/moveable"` x1, `require "engine/node"` x1, `require "engine/object"` x2, `require "engine/particles"` x1, `require "engine/profile"` x1, `require "engine/sprite"` x1, `require "engine/string_packer"` x2, `require "engine/text"` x1, `require "engine/ui"` x1, `require "functions/UI_definitions"` x1, `require "functions/button_callbacks"` x1, `require "functions/common_events"` x1, `require "functions/misc_functions"` x2, `require "functions/state_events"` x1, `require "functions/test_functions"` x1, `require "game"` x1, `require "globals"` x1, `require "love.audio"` x1, `require "love.sound"` x1, `require "love.system"` x3, `require "love.thread"` x1, `require "love.timer"` x1, `require "tag"` x1, `require 'love.filesystem'` x1, `require 'luasteam'` x2, `require('https'` x2
- **love.thread**: `love.thread.getChannel` x12, `love.thread.newThread` x4
- **love.data**: `love.data.compress` x1, `love.data.decompress` x1
- **string.buffer / table.new**: none
