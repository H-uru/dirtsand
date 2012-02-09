-- DirtSand DB initialization script --

\connect dirtsand

SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

CREATE SCHEMA auth;
ALTER SCHEMA auth OWNER TO dirtsand;

CREATE SCHEMA game;
ALTER SCHEMA game OWNER TO dirtsand;

CREATE SCHEMA vault;
ALTER SCHEMA vault OWNER TO dirtsand;

SET search_path = auth, pg_catalog;
SET default_tablespace = '';
SET default_with_oids = false;

CREATE TABLE "Accounts" (
    idx integer NOT NULL,
    "Login" character varying(64) NOT NULL,
    "PassHash" character(40) NOT NULL,
    "AcctUuid" uuid NOT NULL,
    "AcctFlags" integer DEFAULT 0 NOT NULL,
    "BillingType" integer DEFAULT 0 NOT NULL
);
ALTER TABLE auth."Accounts" OWNER TO dirtsand;

CREATE SEQUENCE "Accounts_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER TABLE auth."Accounts_idx_seq" OWNER TO dirtsand;
ALTER SEQUENCE "Accounts_idx_seq" OWNED BY "Accounts".idx;
SELECT pg_catalog.setval('"Accounts_idx_seq"', 1, false);

CREATE TABLE "Players" (
    idx integer NOT NULL,
    "AcctUuid" uuid NOT NULL,
    "PlayerIdx" integer NOT NULL,
    "PlayerName" character varying(40) NOT NULL,
    "AvatarShape" character varying(64) NOT NULL,
    "Explorer" integer DEFAULT 1 NOT NULL
);
ALTER TABLE auth."Players" OWNER TO dirtsand;

CREATE SEQUENCE "Players_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER TABLE auth."Players_idx_seq" OWNER TO dirtsand;
ALTER SEQUENCE "Players_idx_seq" OWNED BY "Players".idx;
SELECT pg_catalog.setval('"Players_idx_seq"', 1, false);

SET search_path = vault, pg_catalog;

CREATE TABLE "NodeRefs" (
    idx integer NOT NULL,
    "ParentIdx" integer NOT NULL,
    "ChildIdx" integer NOT NULL,
    "OwnerIdx" integer DEFAULT 0 NOT NULL,
    "Seen" integer DEFAULT 0 NOT NULL
);
ALTER TABLE vault."NodeRefs" OWNER TO dirtsand;
CREATE SEQUENCE "NodeRefs_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER TABLE vault."NodeRefs_idx_seq" OWNER TO dirtsand;
ALTER SEQUENCE "NodeRefs_idx_seq" OWNED BY "NodeRefs".idx;
SELECT pg_catalog.setval('"NodeRefs_idx_seq"', 1, false);

CREATE TABLE "Nodes" (
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
ALTER TABLE vault."Nodes" OWNER TO dirtsand;
CREATE SEQUENCE "Nodes_idx_seq"
    INCREMENT BY 1
    NO MAXVALUE
    MINVALUE 10001
    CACHE 1;
ALTER TABLE vault."Nodes_idx_seq" OWNER TO dirtsand;
ALTER SEQUENCE "Nodes_idx_seq" OWNED BY "Nodes".idx;
SELECT pg_catalog.setval('"Nodes_idx_seq"', 10001, false);

SET search_path = game, pg_catalog;

CREATE TABLE "Servers" (
    idx integer NOT NULL,
    "AgeUuid" uuid NOT NULL,
    "AgeFilename" character varying(64) NOT NULL,
    "DisplayName" character varying(1024) NOT NULL,
    "AgeIdx" integer NOT NULL,
    "SdlIdx" integer NOT NULL,
    "Temporary" boolean DEFAULT 'f' NOT NULL
);
ALTER TABLE game."Servers" OWNER TO dirtsand;
CREATE SEQUENCE "Servers_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER TABLE game."Servers_idx_seq" OWNER TO dirtsand;
ALTER SEQUENCE "Servers_idx_seq" OWNED BY "Servers".idx;
SELECT pg_catalog.setval('"Servers_idx_seq"', 1, false);

CREATE TABLE "PublicAges" (
    idx integer NOT NULL,
    "AgeUuid" uuid NOT NULL,
    "AgeFilename" character varying(64) NOT NULL,
    "AgeInstName" character varying(64) NOT NULL,
    "AgeUserName" character varying(64) NOT NULL,
    "AgeDesc" character varying(1024) NOT NULL,
    "SeqNumber" integer NOT NULL,
    "Language" integer NOT NULL,
    "CurrentPopulation" integer NOT NULL,
    "Population" integer NOT NULL
);
ALTER TABLE game."PublicAges" OWNER TO dirtsand;
CREATE SEQUENCE "PublicAges_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER TABLE game."PublicAges_idx_seq" OWNER TO dirtsand;
ALTER SEQUENCE "PublicAges_idx_seq" OWNED BY "PublicAges".idx;
SELECT pg_catalog.setval('"PublicAges_idx_seq"', 1, false);

CREATE SEQUENCE "AgeSeqNumber"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER TABLE game."AgeSeqNumber" OWNER TO dirtsand;
SELECT pg_catalog.setval('"AgeSeqNumber"', 1, false);

CREATE TABLE "AgeStates" (
    idx integer NOT NULL,
    "ServerIdx" integer NOT NULL,
    "Descriptor" character varying(64) NOT NULL,
    "ObjectKey" text NOT NULL,
    "SdlBlob" text NOT NULL
);
ALTER TABLE game."AgeStates" OWNER TO dirtsand;
CREATE SEQUENCE "AgeStates_idx_seq"
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;
ALTER TABLE game."AgeStates_idx_seq" OWNER TO dirtsand;
ALTER SEQUENCE "AgeStates_idx_seq" OWNED BY "AgeStates".idx;
SELECT pg_catalog.setval('"AgeStates_idx_seq"', 1, false);

SET search_path = auth, pg_catalog;
ALTER TABLE "Accounts" ALTER COLUMN idx SET DEFAULT nextval('"Accounts_idx_seq"'::regclass);
ALTER TABLE "Players" ALTER COLUMN idx SET DEFAULT nextval('"Players_idx_seq"'::regclass);

ALTER TABLE ONLY "Accounts"
    ADD CONSTRAINT "Accounts_Login_key" UNIQUE ("Login");
ALTER TABLE ONLY "Accounts"
    ADD CONSTRAINT "Accounts_pkey" PRIMARY KEY (idx);
ALTER TABLE ONLY "Players"
    ADD CONSTRAINT "Players_pkey" PRIMARY KEY (idx);
CREATE INDEX "Login_Index" ON "Accounts" USING hash ("Login");

SET search_path = vault, pg_catalog;
ALTER TABLE "NodeRefs" ALTER COLUMN idx SET DEFAULT nextval('"NodeRefs_idx_seq"'::regclass);
ALTER TABLE "Nodes" ALTER COLUMN idx SET DEFAULT nextval('"Nodes_idx_seq"'::regclass);

ALTER TABLE ONLY "NodeRefs"
    ADD CONSTRAINT "NodeRefs_pkey" PRIMARY KEY (idx);
ALTER TABLE ONLY "Nodes"
    ADD CONSTRAINT "Nodes_pkey" PRIMARY KEY (idx);

SET search_path = game, pg_catalog;
ALTER TABLE "Servers" ALTER COLUMN idx SET DEFAULT nextval('"Servers_idx_seq"'::regclass);
ALTER TABLE "PublicAges" ALTER COLUMN idx SET DEFAULT nextval('"PublicAges_idx_seq"'::regclass);
ALTER TABLE "AgeStates" ALTER COLUMN idx SET DEFAULT nextval('"AgeStates_idx_seq"'::regclass);

ALTER TABLE ONLY "Servers"
    ADD CONSTRAINT "Servers_pkey" PRIMARY KEY (idx);
ALTER TABLE ONLY "PublicAges"
    ADD CONSTRAINT "PublicAges_pkey" PRIMARY KEY (idx);
ALTER TABLE ONLY "AgeStates"
    ADD CONSTRAINT "AgeStates_pkey" PRIMARY KEY (idx);

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM postgres;
GRANT ALL ON SCHEMA public TO postgres;
GRANT ALL ON SCHEMA public TO PUBLIC;
