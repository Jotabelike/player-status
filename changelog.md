## 1.0.0 Beta 5
- Fixed a major crash (EXCEPTION_ACCESS_VIOLATION) on the profile page
- Resolved pointer issues during active network requests
- Moved profile status labels down by 5 pixels to prevent overlapping
- Fixed a math error in the server's Argon rate-limit handler
- Reduced the server's global Argon cooldown from 10 minutes to 2 minutes
- Added a 15-second timeout limit to prevent infinite loading
- Truncated massive 502 HTML error logs to keep the Geode console clean
- Resolved the persistent "Network error" on first profile views (Cold Starts)

# 1.0.0 Beta 4
- Implemented Argon token caching on the server to prevent API rate limits (Error 429)
- Added a global cooldown system to gracefully handle traffic spikes and server restarts
- Added detailed error logging to the Geode console for failed status updates to improve debugging

# 1.0.0 Beta 3
- Added editor status (Editor: Level Name) with purple and yellow colors
- Fixed Argon re-authentication when switching accounts without restarting
- Fixed positioning to be relative to profile nodes instead of window size
- Fixed status label duplicating on profile refresh
- Improved immediate status ping when viewing own profile

# 1.0.0 Beta 2
- Argon authentication was added

# 1.0.0 Beta 1
- First Version
- Adds player statuses in online mode