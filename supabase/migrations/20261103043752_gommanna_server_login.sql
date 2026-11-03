-- Separate login for the C++ backend. No password is stored in this file.
-- A project administrator sets its password out of band before first use.
-- The login has no table privileges until it explicitly SET ROLEs to the
-- existing restricted gommanna_server role.
create role gommanna_login login nosuperuser nocreatedb nocreaterole
  noinherit nobypassrls;
grant gommanna_server to gommanna_login;
