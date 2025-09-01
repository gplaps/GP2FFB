# FFB for x86GP2 – BETA 0.3.1
**USE AT YOUR OWN RISK**

This is a custom Force Feedback application for the classic racing simulator **Grand Prix II** by Microprose, and specifically the x86 version update from hatcher:

https://grandprix2.racing/file/misc/view/x86gp2

This app works by reading memory from the game (speed, tire loads, etc.) and uses that to send DirectInput force effects to your wheel.

This project is closely related to the ICR2FFB project and uses the same base code with modifications specific to GP2 and the data available in x86GP2.

Its all a lot of guesswork and workarounds bandaided together but it feels ok!

Big thanks to SK, Eric H, Niels Heusinkveld and Hatcher for all their help in bringing this to life.

---

## DISCLAIMER – USE AT YOUR OWN RISK

I have tried my best to make sure this app can't hurt you or your wheel but **no guarantees**.  
I’ve been using it without issues on my own hardware, but your mileage may vary.

---

## Installation

1. **Download** the app. Get the latest version from Releases
2. **Open `ffb.ini`** and edit the following:
    - `Device` — Must match your device name **exactly** as seen in Windows "Game Controllers".
    - `Force` — Controls force scale (default is 25%).  
      **Be careful** — although the code limits input, always test with low force first.

3. Launch the game and then **Run the app**.  
   After a moment, the window should begin showing telemetry.

> You may need to run the app in **Admin mode** so it can access GP2’s memory.

---

## Changing Settings

You can close the app, edit `ffb.ini`, and reopen it while x86GP2 is still running.  
To avoid sudden force application, **pause the game first** before restarting the app.

---

## Version History

### Betas
**0.3.1 (2025-09-01)** 
- Recaled effects to prevent constant peaking at high load
- Removed some unncessary telemetry from the display

**0.3.0 (2025-08-31)** 
- Reworked force calculation to remove unncessary smoothing which made the center feel 'sloppy'
- Removed low-speed logic which caused the effects to remain on if the car stopped
- Fixed version check on startup so it doesnt matter if you start x86GP2 or the FFB app first

**0.2.2 (2025-08-31)** 
- Fixed memory reading issues which *may* have caused the program to occasionally crash

**0.2.1 (2025-08-31)** 
- Updated version check code
- re-scaled Damper effect to run to a higher speed

**0.2.0 (2025-08-30)** 
- GP2 data pushed through the 'vehicle_dynamics' code to try to convert it into "real" numbers to use
- Code cleanup to remove unncessary ICR2-related things.

**0.1.0 (2025-08-30)** 
- First "working" version

---