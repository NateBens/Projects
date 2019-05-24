# Nathan Benson 'Resourceful' Web Project

### Resource: Videogames
#### Attributes:
* Name
* Genre 
* Size 
* Date 
* Publisher
### Resource: User
#### Attributes:
* Username
* Email
* Password
### Resource: Session
#### Attributes:
* User

### Database Schema
```sql
CREATE TABLE videogames (
id INTEGER PRIMARY KEY,
name TEXT,
genre TEXT,
size REAL,
date TEXT,
publisher TEXT);

CREATE TABLE users(
id INTEGER PRIMARY KEY,
username TEXT,
email TEXT,
password TEXT);
```
### REST endpoints
* Name: List
  * HTTP Method: GET
  * Path: "/videogames"
* Name: Retrieve
  * HTTP Method: GET
  * Path: "/videogames/(id)"
  * Path: "/sessions"
  * Path: "/users"
* Name: Replace
  * HTTP Method: PUT
  * Path: "/videogames/(id)"
* Name: Create
  * HTTP Method: POST
  * Path: "/videogames"
  * Path: "/sessions"
  * Path: "/users"
* Name: Delete
  * HTTP Method: DELETE
  * Path: "/videogames/(id)"
  * Path: "/sessions"
  
# f18-resourceful-Natedog9007
# f18-deploy-Natedog9007
