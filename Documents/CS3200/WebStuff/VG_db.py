#import sqlite3
import os
import psycopg2
import psycopg2.extras
import urllib.parse


def dict_factory(cursor,row):
	d = {}
	for idx, col in enumerate(cursor.description):
		d[col[0]]=row[idx]
	return d

class VGDB:
	def __init__(self):
		urllib.parse.uses_netloc.append("postgres")
		url = urllib.parse.urlparse(os.environ["DATABASE_URL"])

		self.connection = psycopg2.connect(
            cursor_factory=psycopg2.extras.RealDictCursor,
            database=url.path[1:],
            user=url.username,
            password=url.password,
            host=url.hostname,
            port=url.port
        )

		#print("Connecting to DB...")
		#self.connection = sqlite3.connect("VG_db.db")
		#self.connection.row_factory = dict_factory
		
		self.cursor = self.connection.cursor()
		return
	def __del__(self):
		print("Disconnecting from DB.")
		self.connection.close()
		return
	def createVideogamesTable(self):
		self.cursor.execute("CREATE TABLE IF NOT EXISTS videogames (id SERIAL PRIMARY KEY, name VARCHAR(255), genre VARCHAR(255), size INTEGER, date VARCHAR(255), publisher VARCHAR(255))")
		self.connection.commit()
	def createUsersTable(self):
		self.cursor.execute("CREATE TABLE IF NOT EXISTS users (id SERIAL PRIMARY KEY, username VARCHAR(255), email VARCHAR(255), password VARCHAR(255))")
		self.connection.commit()
	def getVG(self,vg_id):
		self.cursor.execute("SELECT * FROM videogames where id = %s",(vg_id,))
		return self.cursor.fetchone()
	def getVGs(self):
		self.cursor.execute("SELECT * FROM videogames" )
		return self.cursor.fetchall()
	def createVG(self,name,genre,size,date,publisher):
		self.cursor.execute("INSERT INTO videogames (name, genre, size, date, publisher) VALUES (%s, %s, %s, %s, %s)", (name,genre,int(size),date,publisher))
		self.connection.commit()
		return
	def deleteVG(self,vg_id):
		self.cursor.execute("DELETE FROM videogames where id = %s",(vg_id,))
		self.connection.commit()
		return
	def updateVG(self,name,genre,size,date,publisher,vg_id):
		self.cursor.execute("UPDATE videogames SET name=%s, genre=%s,size=%s,date=%s,publisher=%s WHERE id = %s",(name,genre,size,date,publisher,vg_id))
		self.connection.commit()
		return
	def getUser(self,user_id):
		self.cursor.execute("SELECT * FROM users where id = %s",(user_id,))
		return self.cursor.fetchone()
	def getUserByEmail(self,email):
		self.cursor.execute("SELECT * FROM users where email = %s",(email,))
		return self.cursor.fetchone()
	def getUsers(self,user_id):
		self.cursor.execute("SELECT * FROM users" )
		return self.cursor.fetchall()
	def createUser(self,username,email,password):
		self.cursor.execute("INSERT INTO users (username,email,password) VALUES (%s, %s, %s)", (username,email,password))
		self.connection.commit()
	def updateUser(self,username,email,password,user_id):
		self.cursor.execute("UPDATE videogames SET username=%s, email=%s,password=%s WHERE id = %s",(username,email,password,user_id))
		self.connection.commit()
		return