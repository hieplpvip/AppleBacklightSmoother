// Add PNLF device for AppleBacklightSmoother.kext
// For Arrandale, Sandy Bridge, Ivy Bridge

DefinitionBlock("", "SSDT", 2, "HIEP", "PNLF", 0)
{
    Scope (_SB.PCI0.GFX0)
    {
        Device (PNLF)
        {
            Name (_ADR, Zero)
            Name (_HID, EisaId ("APP0002"))  // _HID: Hardware ID
            Name (_CID, "backlight")  // _CID: Compatible ID
            Name (_UID, 0x0E)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                If (_OSI ("Darwin"))
                {
                    Return (0x0F)
                }
                Else
                {
                    Return (Zero)
                }
            }
        }
    }
}

