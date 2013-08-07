-- Gearman PostgreSQL Functions
-- Copyright (C) 2009 Eric Day, Selena Deckelmann
--
-- Use and distribution licensed under the BSD license.  See
-- the COPYING file in the parent directory for full text.

SELECT gman_servers_set('localhost');
SELECT gman_do('reverse', 'Hello World!');
SELECT gman_do_high('reverse', 'Hello World!');
SELECT gman_do_low('reverse', 'Hello World!');
SELECT gman_do_background('reverse', 'Hello World!');
SELECT gman_do_high_background('reverse', 'Hello World!');
SELECT gman_do_low_background('reverse', 'Hello World!');
