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

SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

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


-- [Required] Checks to see if a node is in another node's tree --
CREATE OR REPLACE FUNCTION vault.has_node(integer, integer)
RETURNS BOOLEAN AS
$BODY$
DECLARE
    parentId ALIAS FOR $1;
    childId ALIAS FOR $2;
    curNode vault."NodeRefs";
BEGIN
    FOR curNode IN SELECT * FROM vault."NodeRefs" WHERE "ParentIdx"=parentId
    LOOP
        IF curNode."ChildIdx" = childId THEN
            return true;
        ELSEIF vault.has_node(curNode."ChildIdx", childId) THEN
            return true;
        END IF;
    END LOOP;
    return false;
END;
$BODY$
LANGUAGE plpgsql VOLATILE;


-- [Required] Create a new score--
-- This prevents score creation race conditions --
CREATE OR REPLACE FUNCTION auth.create_score(integer, integer, character varying, integer)
RETURNS integer AS
$BODY$
DECLARE
    ownerId ALIAS FOR $1;
    scoreType ALIAS FOR $2;
    scoreName ALIAS FOR $3;
    points ALIAS FOR $4;

    scoreId integer DEFAULT -1;
BEGIN
    IF (SELECT COUNT(*) FROM auth."Scores" WHERE "OwnerIdx"=ownerId AND "Name"=scoreName) < 1 THEN
        INSERT INTO auth."Scores" ("OwnerIdx", "CreateTime", "Type", "Name", "Points")
        VALUES (ownerId, EXTRACT(EPOCH FROM NOW()), scoreType, scoreName, points)
        RETURNING idx INTO scoreId;
    END IF;
    RETURN scoreId;
END;
$BODY$
LANGUAGE plpgsql VOLATILE;
ALTER FUNCTION auth.create_score(IN integer, IN integer, IN character varying, IN integer) OWNER TO dirtsand;

-- [Required] Adds points to a score--
-- This is necessary because some scores must be constrained to positive values --
-- Please note that you are responsible for ensuring the score exists! --
CREATE OR REPLACE FUNCTION auth.add_score_points(integer, integer, boolean)
RETURNS boolean AS
$BODY$
DECLARE
    scoreId ALIAS FOR $1;
    points ALIAS FOR $2;
    allowNegative ALIAS FOR $3;

    dbPoints integer DEFAULT 0;
BEGIN
    SELECT "Points" FROM auth."Scores" WHERE idx = scoreId LIMIT 1 FOR UPDATE INTO dbPoints;
    IF (dbPoints + points >= 0) OR allowNegative THEN
        UPDATE auth."Scores" SET "Points"="Points"+points WHERE idx=scoreId;
        RETURN TRUE;
    ELSE
        UPDATE auth."Scores" SET "Points"=0 WHERE idx=scoreId;
        RETURN FALSE; /* Indicates that we ran out of points */
    END IF;
END;
$BODY$
LANGUAGE plpgsql VOLATILE;


--- [Required] Transfers points from one score to another ---
CREATE OR REPLACE FUNCTION auth.transfer_score_points(integer, integer, integer, boolean)
RETURNS boolean AS
$BODY$
DECLARE
    srcScoreId ALIAS FOR $1;
    dstScoreId ALIAS FOR $2;
    numPoints ALIAS FOR $3;
    allowNegative ALIAS FOR $4;

    dbPoints integer DEFAULT 0;
BEGIN
    SELECT "Points" FROM auth."Scores" WHERE idx = srcScoreId LIMIT 1 FOR UPDATE INTO dbPoints;
    IF (dbPoints - numPoints >= 0) OR allowNegative THEN
        UPDATE auth."Scores" SET "Points"="Points"-numPoints WHERE idx=srcScoreId;
        UPDATE auth."Scores" SET "Points"="Points"+numPoints WHERE idx=dstScoreId;
        RETURN TRUE;
    ELSE
        RETURN FALSE;
    END IF;
END;
$BODY$
LANGUAGE plpgsql VOLATILE;


-- [Optional] Utility to clear an entire vault --
CREATE OR REPLACE FUNCTION clear_vault() RETURNS void AS
$BODY$
SELECT setval('vault."GlobalStates_idx_seq"', 1, false);
SELECT setval('vault."Nodes_idx_seq"', 10001, false);
SELECT setval('vault."NodeRefs_idx_seq"', 1, false);
SELECT setval('game."Servers_idx_seq"', 1, false);
SELECT setval('game."AgeSeqNumber"', 1, false);
SELECT setval('game."AgeStates_idx_seq"', 1, false);
SELECT setval('auth."Players_idx_seq"', 1, false);
SELECT setval('auth."Scores_idx_seq"', 1, false);
DELETE FROM vault."GlobalStates";
DELETE FROM vault."Nodes";
DELETE FROM vault."NodeRefs";
DELETE FROM game."Servers";
DELETE FROM game."AgeStates";
DELETE FROM auth."Players";
DELETE FROM auth."Scores";
$BODY$
LANGUAGE 'sql' VOLATILE;
