-- OpenTibia Server "Evolution 0.7.8" — SQLite schema (DDL only)
--
-- This is the SQLite-dialect equivalent of database.sql (the original MySQL
-- schema, which is the build truth that the SQL IO classes in source/*sql.cpp
-- were written against). Column names and ORDER match database.sql exactly so
-- the original seed INSERTs load positionally.
--
-- Differences vs database.sql, all SQLite-mechanical (behaviour-preserving):
--   * MySQL-only table options (ENGINE=, DEFAULT CHARSET=, COLLATE=) dropped.
--   * `int(11)`/`tinyint`/`unsigned` -> SQLite affinity types (INTEGER/...).
--   * `... auto_increment` + `UNIQUE KEY id` -> `INTEGER PRIMARY KEY AUTOINCREMENT`.
--   * inline MySQL `KEY name (col)` / `USING BTREE` -> separate CREATE INDEX.
--   * NEW table `blessings(player, id)`: queried by source/ioplayersql.cpp
--     (SELECT/DELETE/INSERT) but missing from database.sql — without it the
--     player load fails and the client is rejected on login.
--
-- Regenerate db.s3db from this file with:
--   sqlite3 source/db.s3db < database.sqlite.sql   (then load the seed data)

PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;

CREATE TABLE accounts (
  id        INTEGER PRIMARY KEY AUTOINCREMENT,
  accno     INTEGER     NOT NULL DEFAULT 0,
  password  VARCHAR(32) NOT NULL DEFAULT '',
  type      INTEGER     NOT NULL DEFAULT 0,
  premDays  INTEGER     NOT NULL DEFAULT 0,
  premEnd   INTEGER     NOT NULL DEFAULT 0,
  email     VARCHAR(50) NOT NULL DEFAULT '',
  blocked   INTEGER     NOT NULL DEFAULT 0,
  rlname    VARCHAR(45) NOT NULL DEFAULT '',
  location  VARCHAR(45) NOT NULL DEFAULT '',
  hide      INTEGER     NOT NULL DEFAULT 0,
  hidemail  INTEGER     NOT NULL DEFAULT 0
);
CREATE INDEX idx_accounts_accno ON accounts(accno);

CREATE TABLE addons (
  player  INTEGER NOT NULL DEFAULT 0,
  outfit  INTEGER NOT NULL DEFAULT 0,
  addon   INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE bans (
  type     INTEGER     NOT NULL DEFAULT 0,
  ip       INTEGER     NOT NULL DEFAULT 0,
  mask     INTEGER     NOT NULL DEFAULT 0,
  player   VARCHAR(32) NOT NULL DEFAULT '0',
  account  INTEGER     NOT NULL DEFAULT 0,
  time     INTEGER     NOT NULL DEFAULT 0,
  reason   VARCHAR(50) NOT NULL DEFAULT ''
);

CREATE TABLE blessings (
  player  INTEGER NOT NULL DEFAULT 0,
  id      INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE commands (
  id       INTEGER     NOT NULL DEFAULT 0,
  date     VARCHAR(25) NOT NULL DEFAULT '',
  player   VARCHAR(32) NOT NULL DEFAULT '',
  command  VARCHAR(50) NOT NULL DEFAULT ''
);

CREATE TABLE deathlist (
  player  INTEGER     NOT NULL DEFAULT 0,
  name    VARCHAR(30) NOT NULL DEFAULT '0',
  killer  VARCHAR(30) NOT NULL DEFAULT '0',
  level   INTEGER     NOT NULL DEFAULT 0,
  date    INTEGER     NOT NULL DEFAULT 0
);

CREATE TABLE guildinvited (
  guildid    INTEGER     NOT NULL DEFAULT 0,
  playerid   INTEGER     NOT NULL DEFAULT 0,
  guildrank  VARCHAR(32) NOT NULL DEFAULT ''
);

CREATE TABLE guilds (
  guildid    INTEGER PRIMARY KEY AUTOINCREMENT,
  guildname  VARCHAR(100) NOT NULL DEFAULT '',
  ownerid    INTEGER      NOT NULL DEFAULT 0,
  guildstory TEXT         NOT NULL DEFAULT ''
);

CREATE TABLE houseaccess (
  houseid  INTEGER NOT NULL DEFAULT 0,
  listid   INTEGER          DEFAULT 0,
  list     TEXT
);
CREATE INDEX idx_houseaccess_houseid ON houseaccess(houseid);

CREATE TABLE houses (
  houseid   INTEGER     NOT NULL DEFAULT 0 PRIMARY KEY,
  owner     VARCHAR(32) NOT NULL DEFAULT '0',
  paid      INTEGER              DEFAULT 0,
  warnings  INTEGER              DEFAULT 0
);

CREATE TABLE items (
  player       INTEGER NOT NULL DEFAULT 0,
  slot         INTEGER NOT NULL DEFAULT 0,
  sid          INTEGER NOT NULL DEFAULT 0,
  pid          INTEGER NOT NULL DEFAULT 0,
  type         INTEGER NOT NULL DEFAULT 0,
  number       INTEGER NOT NULL DEFAULT 0,
  actionid     INTEGER NOT NULL DEFAULT 0,
  duration     INTEGER NOT NULL DEFAULT 0,
  charges      INTEGER NOT NULL DEFAULT 0,
  decaystate   INTEGER NOT NULL DEFAULT 0,
  text         TEXT    NOT NULL DEFAULT '',
  specialdesc  TEXT    NOT NULL DEFAULT ''
);
CREATE INDEX idx_items_player ON items(player);

CREATE TABLE players (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  name          VARCHAR(32) NOT NULL DEFAULT '',
  access        INTEGER     NOT NULL DEFAULT 0,
  account       INTEGER     NOT NULL DEFAULT 0,
  level         INTEGER     NOT NULL DEFAULT 0,
  vocation      INTEGER     NOT NULL DEFAULT 0,
  cid           INTEGER     NOT NULL DEFAULT 0,
  health        INTEGER     NOT NULL DEFAULT 0,
  healthmax     INTEGER     NOT NULL DEFAULT 0,
  soul          INTEGER     NOT NULL DEFAULT 0,
  soulmax       INTEGER     NOT NULL DEFAULT 100,
  direction     INTEGER     NOT NULL DEFAULT 0,
  experience    INTEGER     NOT NULL DEFAULT 0,
  lookbody      INTEGER     NOT NULL DEFAULT 0,
  lookfeet      INTEGER     NOT NULL DEFAULT 0,
  lookhead      INTEGER     NOT NULL DEFAULT 0,
  looklegs      INTEGER     NOT NULL DEFAULT 0,
  looktype      INTEGER     NOT NULL DEFAULT 0,
  lookaddons    INTEGER     NOT NULL DEFAULT 0,
  knowaddons    INTEGER     NOT NULL DEFAULT 0,
  maglevel      INTEGER     NOT NULL DEFAULT 0,
  mana          INTEGER     NOT NULL DEFAULT 0,
  manamax       INTEGER     NOT NULL DEFAULT 0,
  manaspent     INTEGER     NOT NULL DEFAULT 0,
  masterpos     VARCHAR(16) NOT NULL DEFAULT '',
  pos           VARCHAR(16) NOT NULL DEFAULT '',
  speed         INTEGER     NOT NULL DEFAULT 0,
  cap           INTEGER     NOT NULL DEFAULT 0,
  maxdepotitems INTEGER     NOT NULL DEFAULT 1000,
  food          INTEGER     NOT NULL DEFAULT 0,
  sex           INTEGER     NOT NULL DEFAULT 0,
  guildid       INTEGER     NOT NULL DEFAULT 0,
  guildrank     VARCHAR(32) NOT NULL DEFAULT '',
  guildnick     VARCHAR(32) NOT NULL DEFAULT '',
  ownguild      INTEGER     NOT NULL DEFAULT 0,
  lastlogin     INTEGER     NOT NULL DEFAULT 0,
  lastip        INTEGER     NOT NULL DEFAULT 0,
  save          INTEGER     NOT NULL DEFAULT 1,
  redskulltime  INTEGER     NOT NULL DEFAULT 0,
  redskull      INTEGER     NOT NULL DEFAULT 0,
  comment       VARCHAR(255) NOT NULL DEFAULT '',
  hide          INTEGER     NOT NULL DEFAULT 0
);
CREATE INDEX idx_players_name ON players(name);

CREATE TABLE playerstorage (
  player  INTEGER NOT NULL DEFAULT 0,
  key     INTEGER NOT NULL DEFAULT 0,
  value   INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX idx_playerstorage_player ON playerstorage(player);

CREATE TABLE reports (
  id      INTEGER PRIMARY KEY AUTOINCREMENT,
  date    VARCHAR(25)  NOT NULL DEFAULT '',
  report  VARCHAR(150) NOT NULL DEFAULT ''
);

CREATE TABLE skills (
  player  INTEGER NOT NULL DEFAULT 0,
  id      INTEGER NOT NULL DEFAULT 0,
  skill   INTEGER NOT NULL DEFAULT 0,
  tries   INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX idx_skills_player ON skills(player);

CREATE TABLE spells (
  player  INTEGER     NOT NULL DEFAULT 0,
  spell   VARCHAR(50) NOT NULL DEFAULT ''
);

CREATE TABLE tileitems (
  tileid      INTEGER NOT NULL DEFAULT 0,
  sid         INTEGER NOT NULL DEFAULT 0,
  pid         INTEGER NOT NULL DEFAULT 0,
  type        INTEGER NOT NULL DEFAULT 0,
  number      INTEGER NOT NULL DEFAULT 0,
  attributes  BLOB
);
CREATE INDEX idx_tileitems_tileid ON tileitems(tileid);

CREATE TABLE tilelist (
  tileid  INTEGER PRIMARY KEY NOT NULL,
  x       INTEGER NOT NULL,
  y       INTEGER NOT NULL,
  z       INTEGER NOT NULL,
  UNIQUE (x, y, z)
);

CREATE TABLE viplist (
  account  INTEGER     NOT NULL DEFAULT 0,
  player   VARCHAR(32) NOT NULL DEFAULT '0'
);
CREATE INDEX idx_viplist_account ON viplist(account);

COMMIT;
