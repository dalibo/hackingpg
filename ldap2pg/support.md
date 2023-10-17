# Atelier ldap2pg

## Objectifs

- démarrer une instance PostgreSQL 
- monter rapidement un annuaire OpenLDAP
- gérer quelques rôles
- gérer quelques privilèges


## Système

- VM
- Bullseye


## PostgreSQL

- apt-get install postgresql
- pg_ctlcluster 13 main start
- sudo -iu postgres
- trust


## OpenLDAP

- apt install slapd-contrib
- dpkg-reconfigure -p low slapd
  - bridoulou.fr
- sudo ldapwhoami -H ldapi:/// -Y external 
- sudo ldapmodify -H ldapi:/// -Y external -f config.ldif
- sudo ldapadd -H ldapi:/// -Y external -f data.ldif
- ldapwhoami -D cn=admin,dc=bridoulou,dc=fr -x -W
- export LDAPBINDDN=cn=admin,dc=bridoulou,dc=fr
- ldapsearch -x -W -b ou=people,dc=bridoulou,dc=fr


## ldap2pg

En root:

- https://apt.dalibo.org/labs/
- wget https://apt.dalibo.org/labs/debian-dalibo.asc
- gpg --dearmor debian-dalibo.asc
- echo deb http://apt.dalibo.org/labs bullseye-dalibo main > /etc/apt/sources.list.d/dalibo.list
- apt update
- apt install ldap2pg
- ldap2pg --version


## ldap2pg

En utilisateur `postgres`:

- export LDAPURI=ldap://localhost:389 LDAPBINDDN=cn=admin,dc=bridoulou,dc=fr LDAPPASSWORD=
- ldapwhoami -x -w "$LDAPPASSWORD"


## Étape 1

``` yaml
version: 6

rules:
- role: alice
```

- ldap2pg
- ldap2pg --verbose
- ldap2pg --real
- ldap2pg  # Nothing to do.
