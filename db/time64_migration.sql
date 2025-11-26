-- Unfortunately, dirtsand doesn't have built-in support for migrations yet.
-- Therefore, in order to make the DB Y2038 compliant, this migration must be
-- manually run on the dirtsand database.
-- Depending on the size of the Vault database, this may take a while to
-- complete, and will require exclusive access to the Nodes table for the
-- duration of the migration.

BEGIN TRANSACTION;

ALTER TABLE vault."Nodes"
    ALTER COLUMN "CreateTime" TYPE bigint,
    ALTER COLUMN "ModifyTime" TYPE bigint;

COMMIT;
