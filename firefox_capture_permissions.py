import configparser
import pathlib
import sqlite3

base = pathlib.Path('/boot/home/config/settings/Mozilla/Firefox')
config = configparser.ConfigParser()
config.read(base / 'profiles.ini')
for section in config.sections():
    if not section.startswith('Profile'):
        continue
    folder = base / config[section]['Path']
    database = folder / 'permissions.sqlite'
    if not database.exists():
        continue
    print('Profile', config[section].get('Name', section))
    # Immutable mode inspects persisted permissions without competing with
    # Firefox's exclusive database lock; uncheckpointed WAL entries are absent.
    with sqlite3.connect(database.as_uri() + '?mode=ro&immutable=1', uri=True) as connection:
        rows = connection.execute("SELECT origin,type,permission,expireType FROM moz_perms WHERE origin LIKE '%meet.google.com%' AND type IN ('camera','microphone')").fetchall()
        print('Meet capture permissions:', rows)
