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
-- Utility functions for DirtSand operation and administration --

\connect dirtsand

SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

-- [Required] These functions require the plpgsql language --
-- If this line fails, make sure plpgsql isn't already created in the target db
-- (9.0 and later versions of Postgres support CREATE OR REPLACE LANGUAGE)
CREATE LANGUAGE plpgsql;

-- [Required] Fetch a complete noderef tree, for the FetchNodeRefs message --
CREATE OR REPLACE FUNCTION vault.fetch_tree(integer)
RETURNS SETOF vault."NodeRefs" AS
$BODY$
DECLARE
    nodeId ALIAS FOR $1;
    curNode vault."NodeRefs";
BEGIN
    FOR curNode IN SELECT * FROM vault."NodeRefs" WHERE "ParentIdx"=nodeId
    LOOP
        RETURN NEXT curNode;
        RETURN QUERY (SELECT * FROM vault.fetch_tree(curNode."ChildIdx"));
    END LOOP;
    RETURN;
END;
$BODY$
LANGUAGE plpgsql VOLATILE;
ALTER FUNCTION vault.fetch_tree(IN integer) OWNER TO dirtsand;


-- [Required] Fetch a specific folder child node --
CREATE OR REPLACE FUNCTION vault.find_folder(integer, integer)
RETURNS SETOF vault."Nodes" AS
$BODY$
DECLARE
    nodeId ALIAS FOR $1;
    folderType ALIAS FOR $2;
    curNode vault."Nodes";
BEGIN
    FOR curNode IN SELECT * FROM vault."Nodes" WHERE idx IN
        (SELECT "ChildIdx" FROM vault."NodeRefs" WHERE "ParentIdx"=nodeId)
        AND ("NodeType"=22 OR "NodeType"=30 OR "NodeType"=34)
        AND "Int32_1"=folderType
    LOOP
        RETURN NEXT curNode;
    END LOOP;
    RETURN;
END;
$BODY$
LANGUAGE plpgsql VOLATILE;
ALTER FUNCTION vault.find_folder(IN integer, IN integer) OWNER TO dirtsand;


-- [Optional] Utility to clear an entire vault --
CREATE OR REPLACE FUNCTION clear_vault() RETURNS void AS
$BODY$
SELECT setval('vault."Nodes_idx_seq"', 10001, false);
SELECT setval('vault."NodeRefs_idx_seq"', 1, false);
SELECT setval('game."Servers_idx_seq"', 1, false);
SELECT setval('game."PublicAges_idx_seq"', 1, false);
SELECT setval('game."AgeSeqNumber"', 1, false);
SELECT setval('game."AgeStates_idx_seq"', 1, false);
SELECT setval('auth."Players_idx_seq"', 1, false);
DELETE FROM vault."Nodes";
DELETE FROM vault."NodeRefs";
DELETE FROM game."Servers";
DELETE FROM game."PublicAges";
DELETE FROM game."AgeStates";
DELETE FROM auth."Players";
$BODY$
LANGUAGE 'sql' VOLATILE;
ALTER FUNCTION clear_vault() OWNER TO dirtsand;
