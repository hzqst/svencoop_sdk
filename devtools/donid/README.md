# Player extras and bans

Sven Co-op has [donid](/dlls/donid.cpp) system which gives some extras or bans player entirely from any game servers he joins, even their local one. Of course, if server's game DLL cares indeed.
Two instances of it exist: offline + online.

`players.csv` here contains ~5300 SteamIDs, including 114 banned ones.

That thing has the same four columns as the embedded list: `auth_id,name,description,level`.
Version: `5.26`

Fun fact: at some point (probably 5.25->5.26) offline list stopped being plain text in binary.
DonId (or whatever) data is now Base64-encoded gzip-compressed CSV string (donid_data.h).

For online extras list, devs use RC4 encryption.

Anyway, there is ready extracted list and Python script to read donid data header and also generate it.

Run from repo root with Python (depends on your situation):
```
python3 devtools/donid/donid_data.py generate devtools/donid/players.csv --output dlls/donid_data.h
python3 devtools/donid/donid_data.py read dlls/donid_data.h --output devtools/donid/players.csv
```

If you're curious what extras level means:

| Level | Meaning |
| --- | --- |
| -1 | Banned (barred xd) |
| 0 | None |
| 1 | Donor (electric crowbar and Chumtoad) |
| 2 | Donor (gold Uzi weapon) |
| 3 | Donor (increased Chumtoad health) |
| 4 | Staff (contributor/tester) |
| 5 | Staff (artist) |
| 6 | Staff (leader/developer) |
