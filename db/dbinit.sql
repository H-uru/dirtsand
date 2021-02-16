-- This file is part of dirtsand.
--
-- dirtsand is free software: you can redistribute it and/or modify
-- it under the terms of the GNU Affero General Public License as
-- published by the Free Software Foundation, either version 3 of the
-- License, or (at your option) any later version.
--
-- dirtsand is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU Affero General Public License for more details.
--
-- You should have received a copy of the GNU Affero General Public License
-- along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.
-------------------------------------------------------------------------------
-- DirtSand DB initialization script --

SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

CREATE SCHEMA auth;
CREATE SCHEMA game;
CREATE SCHEMA vault;

SET search_path = auth, pg_catalog;
SET default_tablespace = '';
SET default_with_oids = false;

CREATE TABLE IF NOT EXISTS "Accounts" (
    idx integer NOT NULL,
    "Login" character varying(64) NOT NULL,
    "PassHash" character(40) NOT NULL,
    "AcctUuid" uuid NOT NULL,
    "AcctFlags" integer DEFAULT 0 NOT NULL,
    "BillingType" integer DEFAULT 0 NOT NULL
);

CREATE SEQUENCE IF NOT EXISTS "Accounts_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER SEQUENCE "Accounts_idx_seq" OWNED BY "Accounts".idx;

CREATE TABLE IF NOT EXISTS "Players" (
    idx integer NOT NULL,
    "AcctUuid" uuid NOT NULL,
    "PlayerIdx" integer NOT NULL,
    "PlayerName" character varying(40) NOT NULL,
    "AvatarShape" character varying(64) NOT NULL,
    "Explorer" integer DEFAULT 1 NOT NULL
);

CREATE SEQUENCE IF NOT EXISTS "Players_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER SEQUENCE "Players_idx_seq" OWNED BY "Players".idx;

CREATE TABLE IF NOT EXISTS "Scores" (
    idx integer NOT NULL,
    "CreateTime" integer DEFAULT 0 NOT NULL,
    "OwnerIdx" integer NOT NULL,
    "Type" integer DEFAULT 1 NOT NULL,
    "Name" character varying(64) NOT NULL,
    "Points" integer DEFAULT 0 NOT NULL
);
CREATE INDEX IF NOT EXISTS "Score_Index" ON auth."Scores" ("OwnerIdx", "Name");

CREATE SEQUENCE IF NOT EXISTS "Scores_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER SEQUENCE "Scores_idx_seq" OWNED BY "Scores".idx;

SET search_path = vault, pg_catalog;

CREATE TABLE IF NOT EXISTS "GlobalStates" (
    idx integer NOT NULL,
    "Descriptor" character varying(64) NOT NULL,
    "SdlBlob" text NOT NULL
);
CREATE INDEX IF NOT EXISTS "GlobalStates_Index" ON vault."GlobalStates" ("Descriptor");

CREATE SEQUENCE IF NOT EXISTS "GlobalStates_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER SEQUENCE "GlobalStates_idx_seq" OWNED BY "GlobalStates".idx;

CREATE TABLE IF NOT EXISTS "NodeRefs" (
    idx integer NOT NULL,
    "ParentIdx" integer NOT NULL,
    "ChildIdx" integer NOT NULL,
    "OwnerIdx" integer DEFAULT 0 NOT NULL
);
CREATE INDEX IF NOT EXISTS "RefParent" ON vault."NodeRefs" ("ParentIdx");
CREATE SEQUENCE "NodeRefs_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER SEQUENCE "NodeRefs_idx_seq" OWNED BY "NodeRefs".idx;

CREATE TABLE IF NOT EXISTS "Nodes" (
    idx integer NOT NULL,
    "CreateTime" integer DEFAULT 0 NOT NULL,
    "ModifyTime" integer DEFAULT 0 NOT NULL,
    "CreateAgeName" character varying(64),
    "CreateAgeUuid" uuid,
    "CreatorUuid" uuid DEFAULT '00000000-0000-0000-0000-000000000000'::uuid NOT NULL,
    "CreatorIdx" integer DEFAULT 0 NOT NULL,
    "NodeType" integer NOT NULL,
    "Int32_1" integer,
    "Int32_2" integer,
    "Int32_3" integer,
    "Int32_4" integer,
    "Uint32_1" integer,
    "Uint32_2" integer,
    "Uint32_3" integer,
    "Uint32_4" integer,
    "Uuid_1" uuid,
    "Uuid_2" uuid,
    "Uuid_3" uuid,
    "Uuid_4" uuid,
    "String64_1" character varying(64),
    "String64_2" character varying(64),
    "String64_3" character varying(64),
    "String64_4" character varying(64),
    "String64_5" character varying(64),
    "String64_6" character varying(64),
    "IString64_1" character varying(64),
    "IString64_2" character varying(64),
    "Text_1" character varying(1024),
    "Text_2" character varying(1024),
    "Blob_1" text,
    "Blob_2" text
);
CREATE INDEX IF NOT EXISTS "PublicAgeList" ON vault."Nodes" ("NodeType", "Int32_2", "String64_2");
CREATE SEQUENCE IF NOT EXISTS "Nodes_idx_seq"
    INCREMENT BY 1
    NO MAXVALUE
    MINVALUE 10001
    CACHE 1;
ALTER SEQUENCE "Nodes_idx_seq" OWNED BY "Nodes".idx;

SET search_path = game, pg_catalog;

CREATE TABLE IF NOT EXISTS "Servers" (
    idx integer NOT NULL,
    "AgeUuid" uuid NOT NULL,
    "AgeFilename" character varying(64) NOT NULL,
    "DisplayName" character varying(1024) NOT NULL,
    "AgeIdx" integer NOT NULL,
    "SdlIdx" integer NOT NULL,
    "Temporary" boolean DEFAULT 'f' NOT NULL
);
CREATE SEQUENCE IF NOT EXISTS "Servers_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER SEQUENCE "Servers_idx_seq" OWNED BY "Servers".idx;

CREATE SEQUENCE IF NOT EXISTS "AgeSeqNumber"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;

CREATE TABLE IF NOT EXISTS "AgeStates" (
    idx integer NOT NULL,
    "ServerIdx" integer NOT NULL,
    "Descriptor" character varying(64) NOT NULL,
    "ObjectKey" text NOT NULL,
    "SdlBlob" text NOT NULL
);
CREATE SEQUENCE IF NOT EXISTS "AgeStates_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER SEQUENCE "AgeStates_idx_seq" OWNED BY "AgeStates".idx;

SET search_path = auth, pg_catalog;
ALTER TABLE "Accounts" ALTER COLUMN idx SET DEFAULT nextval('"Accounts_idx_seq"'::regclass);
ALTER TABLE "Players" ALTER COLUMN idx SET DEFAULT nextval('"Players_idx_seq"'::regclass);
ALTER TABLE "Scores" ALTER COLUMN idx SET DEFAULT nextval('"Scores_idx_seq"'::regclass);

ALTER TABLE ONLY "Accounts"
    ADD CONSTRAINT "Accounts_Login_key" UNIQUE ("Login");
ALTER TABLE ONLY "Accounts"
    ADD CONSTRAINT "Accounts_pkey" PRIMARY KEY (idx);
ALTER TABLE ONLY "Players"
    ADD CONSTRAINT "Players_pkey" PRIMARY KEY (idx);
ALTER TABLE ONLY "Scores"
    ADD CONSTRAINT "Scores_pkey" PRIMARY KEY (idx);
CREATE INDEX IF NOT EXISTS "Login_Index" ON "Accounts" USING hash ("Login");

SET search_path = vault, pg_catalog;
ALTER TABLE "GlobalStates" ALTER COLUMN idx SET DEFAULT nextval('"GlobalStates_idx_seq"'::regclass);
ALTER TABLE "NodeRefs" ALTER COLUMN idx SET DEFAULT nextval('"NodeRefs_idx_seq"'::regclass);
ALTER TABLE "Nodes" ALTER COLUMN idx SET DEFAULT nextval('"Nodes_idx_seq"'::regclass);

ALTER TABLE ONLY "GlobalStates"
    ADD CONSTRAINT "GlobalStates_pkey" PRIMARY KEY (idx);
ALTER TABLE ONLY "NodeRefs"
    ADD CONSTRAINT "NodeRefs_pkey" PRIMARY KEY (idx);
ALTER TABLE ONLY "Nodes"
    ADD CONSTRAINT "Nodes_pkey" PRIMARY KEY (idx);

SET search_path = game, pg_catalog;
ALTER TABLE "Servers" ALTER COLUMN idx SET DEFAULT nextval('"Servers_idx_seq"'::regclass);
ALTER TABLE "AgeStates" ALTER COLUMN idx SET DEFAULT nextval('"AgeStates_idx_seq"'::regclass);

ALTER TABLE ONLY "Servers"
    ADD CONSTRAINT "Servers_pkey" PRIMARY KEY (idx);
ALTER TABLE ONLY "AgeStates"
    ADD CONSTRAINT "AgeStates_pkey" PRIMARY KEY (idx);
