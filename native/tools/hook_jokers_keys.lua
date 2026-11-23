-- prints the keys of the 150 jokers (HOOK=<this file> on the oracle): tools/jokers_all.sh's list
local frame = ...
if frame == 30 then
  for _, c in ipairs(G.P_CENTER_POOLS.Joker) do print('KEY ' .. c.key) end
  io.stdout:flush(); os.exit(0)
end
