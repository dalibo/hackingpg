# Atelier ldap2pg

## Objectifs

- démarrer une instance PostgreSQL 
- monter rapidement un annuaire OpenLDAP
- gérer quelques rôles
- gérer quelques privilèges
- fichiers de l'atelier: https://github.com/dalibo/hackingpg/tree/main/ldap2pg


## Prérequis de l'atelier

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
- mv debian-dalibo.asc.gpg /etc/apt/trusted.gpg.d/
- echo deb http://apt.dalibo.org/labs bullseye-dalibo main > /etc/apt/sources.list.d/dalibo.list
- apt update
- apt install ldap2pg
- ldap2pg --version


## ldap2pg

En utilisateur `postgres`:

- export LDAPURI=ldap://localhost:389 LDAPBINDDN=cn=admin,dc=bridoulou,dc=fr LDAPPASSWORD=...
- ldapwhoami -x -w "$LDAPPASSWORD"
- psql


## Étape 1 - le fichier

``` yaml
# ldap2pg.yml
version: 6

rules:
- role: alice
```

- ldap2pg
- ldap2pg --verbose
- ldap2pg --real
- ldap2pg  # Nothing to do.
- `\du+`


## Étape 2 - les rôles

``` yaml
version: 6

rules:
- roles:
  - name: readers
  - name: alice
    options: LOGIN
    parents: readers
```

- https://ldap2pg.rtfd.io/en/latest/config/


## Étape 3 - recherche LDAP

``` yaml
version: 6

rules:
- description: "Tous les groupes."
  ldapsearch:
    base: ou=groups,dc=bridoulou,dc=fr
  roles:
  - name: "{cn}"
  - name: "{member.cn}"
    parent:
    - "{cn}"
    - pg_monitor
```

- ldap2pg --verbose


## Étape 4 - privileges

``` yaml
version: 6

privileges:
  lecture:
  - type: CONNECT
    on: DATABASE
  - type: USAGE
    on: SCHEMA
 
rules:
- description: "Tous les groupes."
  ldapsearch:
    base: ou=groups,dc=bridoulou,dc=fr
  roles:
  - name: "{cn}"
  - name: "{member.cn}"
    parent: "{cn}"
- grants:
  - role: readers
    privilege: lecture
```

- concept d'ACL : une requête d'inspection d'une colonne `*acl` dans un catalogue système associé à une requête GRANT et une requête REVOKE pour manipuler cet ACL.
