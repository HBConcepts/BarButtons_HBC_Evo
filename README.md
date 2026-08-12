# BarButtons_HBC_Evo

This is a update to the original BarButtons code by JaxeADV ( https://jaxeadv.com/barbuttons/ ). It is based on the publicly available v1 of their code, governed by the Creative Commons Attribution-NonCommercial 4.0 International License.

The main change is that it replaces the 4 buttons originally used for directions with a 5-way switch (10x10x9mm SMD 5-way switch).
The center click of the thumb stick is unused (too hard to use reliably while riding).

This work is also licensed under the Creative Commons Attribution-NonCommercial 4.0 International License. To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

You can find the 3D printable models for an updated layout at https://cults3d.com/en/users/HBConcepts/3d-models

You can flash this code to a LOLIN C3 Mini using the Arduino IDE available at https://www.arduino.cc/en/software/

The libraies needed are the Keypad library and the HijelHID_BLEKeyboard library, both available from the library manager in the Arduino IDE.

# Key Map (works with the Kurviger app)
- Joystick: Pan Map (sends arrow keys)
- Zoom Buttons: Zoom (sends '+' or '-')
- Alt1 (A) button: 
    - Short Press: Menu (sends 'a')
    - Long Press: Switch Map Orientation (3D / 2D / NorthUp) (sends 'b')
- Alt2 (B) button:
    - Short Press: Center map (sends 'c')
    - Long Press: Sends F1 (can be used with the Key Mapper app to tap the "Recalculate" on-screen button in Kurviger)

# Connection

IO pin map:

| IO Pin | Connection |
| --- | --- |
| 0 | +, -, A, B |
| 1 | 5-way switch Common pin |
| 2 | U, + |
| 3 | D, - |
| 4 | L, A |
| 5 | R, B |
| 6 | Joystick Center |
| 7 | LED PWR |

