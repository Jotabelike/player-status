# Player Status

See what your friends are doing in **real time** — right from their profile page.

This mod adds a **live status indicator** to every player's profile, so you can instantly know if they're online, playing a level, editing, or offline.

---

## Statuses

### Online
The player has the game open and is browsing menus (main menu, level browser, etc.).

### Playing: Level Name - 77%
The player is currently inside a level. The level name and their **best percentage** on that level are displayed. The percentage updates if they beat their record during the session.

### Editor: Level Name
The player is currently editing a level. The level name is displayed in yellow next to the purple "Editor:" label.

### Offline
The player is not currently in the game. This status appears when:
- The player **closed the game**
- The player **does not have this mod installed**
- The player has **no internet connection**
- The server **could not be reached**

### Network Error
There was a problem connecting to the status server. This is usually temporary and resolves on its own.

---

## How It Works

- When you open the game, the mod sends your status to a server
- A **heartbeat** is sent every few seconds to keep your status alive
- When you visit someone's profile, the mod checks their status from the server
- If no heartbeat is received for a while, the player is considered **offline**

---

## Privacy

- Only your **username**, **account ID**, **current level name**, and **best percentage** are shared
- Your status is **automatically removed** from the server shortly after closing the game
- **No personal data** is stored permanently

---

## Notes

- Both players need the mod installed to see each other's status
- An internet connection is required for the mod to work
- Status updates may take a few seconds to appear