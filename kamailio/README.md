This is a tutorial on how to setup [Kamailio Open Source SIP server](https://www.kamailio.org/) for use with Kvazzup. Currently adding users is done manually, but other alternatives may exist. This tutorial uses PostgreSQL but other databases are also possible.

## Installation

Install Kamailio and postgress module with following command:
```
apt-get install kamailio kamailio-postgres-modules postgresql postgresql-contrib kamailio-presence-modules kamailio-tls-modules
```

## Configure script Variables
Configuration file:
```
/etc/kamailio/kamctlrc
```

Set SIP_DOMAIN to Kamailio host’s public IP address or domain name.
```
SIP_DOMAIN=10.0.0.2
```

Next, find and set DBENGINE to PGSQL
```
DBENGINE=PGSQL
```

## Create PostgreSQL database

Next we start creating the database. Create a .pgpass file in home folder with postgres user and password: 
```
*:*:*:postgres:postgres
```
and change the permissions with:
```
chmod 0600 ~/.pgpass
```
These credentials are used by kamailio when operating with postgreSQL database. For security reasons, you should NOT change the UNIX password of postgres user, because this unlocks it and makes logging with it possible.

Instead, set the postgres password within psql:
```
sudo -u postgres psql postgres
# \password postgres
```

Create the database in postgres with following command. Change the owner of the database to kamailio.
```
kamdbctl create
```

Check existance of database with
```
sudo -u postgres psql
```

Inside psql list databases, connect to kamailio database and list all tables:
```
psql> \list
psql> \c kamailio
psql> \dt
```

## Configure run-time behavior and modules
Configuration file:
```
/etc/kamailio/kamailio.cfg 
```

Add the following lines to enable postgreSQL, user authentication and persistent REGISTER statuses after restart.
```
#!define WITH_PGSQL
#!define WITH_AUTH
#!define WITH_USRLOCDB
#!define WITH_TLS
```

Add database location to "Defined Values":
```
#!ifdef WITH_PGSQL
#!ifndef DBURL
#!define DBURL "postgres://postgres:postgres@localhost/kamailio"
#!endif
#!endif
```

Add PostgreSQL module:

```
#!ifdef WITH_PGSQL
loadmodule "db_postgres.so"
#!endif
```

Set alias to host’s public IP or domain name and port:
```
alias="10.0.0.2:5060"
```

Set tcp connection to disconnect if no messages are sent. Depends on the registaration frequency of software.
```
tcp_connection_lifetime=610
```

Listen to only the desired interfaces with:
```
listen=tcp:10.0.0.2:5060
listen=tcp:10.0.0.2:5061
```

## Configure GRUU

Change the following parameter to 1 so the REGISTER-request will generate GRUUs:
```
modparam("registrar", "gruu_enabled", 0)
```

The default configuration does not use GRUU to route requests within a dialog by default. Change the first `route(RELAY)` in `route[WITHINDLG]` to the following:

```
if ( is_gruu() ) {
    route(LOCATION);
} else {
    route(RELAY);
}
```

TODO: Something is needed for NATs?

## Startup configuration

Configuration file:
```
/etc/default/kamailio
```

Set user
```
RUN_KAMAILIO=yes
USER=kamailio
GROUP=kamailio
```

## Securing Kamailio

See [Overview of Security related config snippets](https://www.kamailio.org/wiki/tutorials/security/kamailio-security) from Kamailio for security tips.

## Adding a test user

In order for REGISTER-requests to be accepted the user has to exist in database. WITH_AUTH makes kamailio require password. Default password for kamailio is "kamailiorw". Add user with following command: 
```
kamctl add testuser testpasswd
```

(check more commands from [here](https://manpages.debian.org/stretch/kamailio/kamctl.8.en.html))

You can check that everything was successful with interactive terminal:
```
sudo -u postgres psql
```
You can find users in subscriber table and active registrations in location table with select:
```
\c kamailio
\x on
psql> select * from subscriber;
psql> select * from location;
```
## Starting Kamailio

After modifying the config, you can start Kamailio with:
```
systemctl start kamailio
```

You can check the status with:
```
systemctl status kamailio
```
If Kamailio was running before modifying configs, use "systemctl restart" instead.


## Debugging

In case you need to debug Kamailio use the following:
```
journalctl -u kamailio.service
```

You may want to include 
```
#!define WITH_DEBUG
```
at the beginning of /etc/kamailio/kamctlrc for more detailed logs.

## Resources
[Kamailio SIP Trunk Registration](https://telnyx.com/resources/sip-trunk-registration-kamailio) <br>
[Connect Kamailio with PostgreSQL](https://stackoverflow.com/questions/48735686/connect-kamailio-with-postgresql) <br>
[PostgreSQL module documentation](https://www.kamailio.org/docs/modules/devel/modules/db_postgres.html) <br>
[PostgreSQL Documentation](https://www.postgresql.org/docs/current/tutorial-install.html) <br>