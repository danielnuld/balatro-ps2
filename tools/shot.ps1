# Press PCSX2's screenshot key (Screenshot = Keyboard/F8 in PCSX2.ini) without focusing its window.
# Output: Documents\PCSX2\snaps\*.png. Called by tools/pcsx2.sh when the ELF prints a SHOT line.
# With arguments, any other key instead, held for some milliseconds: the pad's keys of PCSX2.ini ([Pad1]), e.g.
#   shot.ps1 0x27 0x14D 100   Right (d-pad right; 0x100 in the scan code = an extended key)
#   shot.ps1 0x4B 0x25 100    K (cross)
param([int]$vk = 0x77, [int]$scan = 0x42, [int]$ms = 0)   # default: F8
Add-Type @"
using System; using System.Runtime.InteropServices;
public class PK {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
"@
$h = (Get-Process pcsx2-qt | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1).MainWindowHandle
$l = 1 -bor ($scan -shl 16)   # bit 24 = extended, from 0x100 of the scan code
[PK]::PostMessage($h, 0x0100, [IntPtr]$vk, [IntPtr]$l) | Out-Null                  # WM_KEYDOWN
Start-Sleep -Milliseconds $ms
[PK]::PostMessage($h, 0x0101, [IntPtr]$vk, [IntPtr]($l -bor 0xC0000000L)) | Out-Null  # WM_KEYUP
