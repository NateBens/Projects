from http.server import BaseHTTPRequestHandler , HTTPServer
from urllib.parse import parse_qs
import json
import sys
from http import cookies
from passlib.hash import bcrypt
from VG_db import VGDB
from sessionStore import SessionStore

valid_ids = [] 
gSessionStore = SessionStore()



class Handler (BaseHTTPRequestHandler):
	def checkID(self,vg_id):
		db = VGDB()
		db.createVideogamesTable()
		videogames= db.getVGs()
		valid_ids = []
		for game in videogames:
			if str(vg_id) == str(game['id']):
				return True
		return False

	def load_cookie(self):
		if "Cookie" in self.headers:
			self.cookie = cookies.SimpleCookie(self.headers["Cookie"])
		else:
			self.cookie = cookies.SimpleCookie()

	def send_cookie(self):
		for morsel in self.cookie.values():
			self.send_header("Set-Cookie",morsel.OutputString())

	def load_session(self):
		self.load_cookie()
		if "sessionId" in self.cookie:
			sessionId = self.cookie["sessionId"].value
			self.session = gSessionStore.getSession(sessionId)
			if self.session == None:
				sessionId = gSessionStore.createSession()
				self.session = gSessionStore.getSession(sessionId)
				self.cookie["sessionId"] = sessionId
		else:
			sessionId = gSessionStore.createSession()
			self.session = gSessionStore.getSession(sessionId)
			self.cookie["sessionId"] = sessionId
		print("CURRENT SESSION", self.session)

	def end_headers(self):
		self.send_cookie()
		self.send_header("Access-Control-Allow-Origin",self.headers["Origin"])
		self.send_header("Access-Control-Allow-Credentials","true")
		BaseHTTPRequestHandler.end_headers(self)

	def do_OPTIONS(self):
		self.load_session()
		self.send_response(200)
		self.send_header("Access-Control-Allow-Headers","Content-Type")
		self.send_header("Access-Control-Allow-Methods","GET,POST,PUT,DELETE,OPTIONS")
		self.end_headers()
		return

	def do_GET(self):
		self.load_session()
		print("PATH: ", self.path)
		if self.path == "/videogames":
			self.handleVGList()
		elif self.path == "/sessions":
			self.handleSessionRetrieve()
		elif self.path.startswith("/videogames/"):
			parts = self.path.split("/")
			VG_id = parts[2]
			if self.checkID(VG_id):
				self.handleVGRetrieve(VG_id)
			else:
				self.send404()
		elif self.path == "/users":
			self.handleUserRetrieve()
		else:
			self.send404()

	def do_POST(self):
		self.load_session()
		if self.path == "/videogames":
			self.handleVGcreate()
		elif self.path == "/users":
			self.handleUserCreate()
		elif self.path == "/sessions":
			self.handleSessionCreate()
		else:
			self.send404()
	def do_PUT(self):
		self.load_session()
		if "userId" not in self.session:
			self.handle401()
			return
		self.load_session()
		if self.path.startswith("/videogames/"):
			parts = self.path.split("/")
			VG_id = parts[2]
			if self.checkID(VG_id):
				self.handleVGUpdate(VG_id)
			else:
				self.send404()
		else:
			self.send404()

	def do_DELETE(self):
		self.load_session()
		print("PATH: ", self.path)
		if self.path == "/videogames":
			self.send_response(405)
			self.send_header("content-type","text/html")
			self.end_headers()
			self.wfile.write(bytes("<h1>405 Not allowed to delete parent directory.</h1>", "utf-8"))
		elif self.path.startswith("/videogames/"):
			parts = self.path.split("/")
			VG_id = parts[2]
			if self.checkID(VG_id):
				self.handleVGDelete(VG_id)
			else:
				self.send404()
		elif self.path == "/sessions":
			self.handleSessionDelete()

		else:
			self.send404()

	def handleVGcreate(self):
		if "userId" not in self.session:
			self.handle401()
			return
		length = self.headers["Content-length"]
		body = self.rfile.read(int(length)).decode("utf-8")
		print("THE BODY: ", body,"\n")
		data = parse_qs(body)
		name = data['name'][0]
		genre = data['genre'][0]
		size = data['size'][0]
		date = data['date'][0]
		publisher = data['publisher'][0]
		#print("Name: " + str(name)+"\n")
		db = VGDB()
		db.createVideogamesTable()
		db.createVG(name,genre,size,date,publisher)
		self.send_response(201)
		self.end_headers()

	def handleVGList(self):
		if "userId" not in self.session:
			self.handle401()
			return
		print(self.session)
		self.send_response(200)
		self.send_header("content-type","application/json")
		self.end_headers()
		db = VGDB()
		db.createVideogamesTable()
		videogames= db.getVGs()
		self.wfile.write(bytes(json.dumps(videogames), "utf-8"))
	def handleVGRetrieve(self,VG_id):
		if "userId" not in self.session:
			self.handle401()
			return
		self.send_response(200)
		self.send_header("content-type","application/json")
		self.end_headers()
		db = VGDB()
		db.createVideogamesTable()
		videogame = db.getVG(VG_id)
		self.wfile.write(bytes(json.dumps(videogame), "utf-8"))
		print(videogame)
	def handleVGDelete(self,VG_id):
		if "userId" not in self.session:
			self.handle401()
			return
		db = VGDB()
		db.createVideogamesTable()
		db.deleteVG(VG_id)

		self.send_response(200)
		self.end_headers()
		
		#videogames= db.getVGs()
		#print(videogames)
	def handleVGUpdate(self,VG_id):
		if "userId" not in self.session:
			self.handle401()
			return
		self.send_response(200)
		self.send_header("content-type","application/json")
		self.end_headers()
		length = self.headers["Content-length"]
		body = self.rfile.read(int(length)).decode("utf-8")
		print("THE BODY: ", body,"\n")
		data = parse_qs(body)
		name = data['name'][0]
		genre = data['genre'][0]
		size = data['size'][0]
		date = data['date'][0]
		publisher = data['publisher'][0]
		#print("Name: " + str(name)+"\n")
		db = VGDB()
		db.createVideogamesTable()
		db.updateVG(name,genre,size,date,publisher,VG_id)
		#videogames= db.getVGs()
		#print(videogames)
	def handleSessionRetrieve(self):
		if "userId" in self.session:
			db = VGDB()
			db.createVideogamesTable()
			user = db.getUser(self.session["userId"])
			self.send_response(200)
			self.send_header("content-type","application/json")
			self.end_headers()
			self.wfile.write(bytes(json.dumps(user), "utf-8"))
		else:
			self.handle401()
	def handleSessionCreate(self):
		length = self.headers["content-length"]
		body = self.rfile.read(int(length)).decode("utf-8")
		data = parse_qs(body)

		email = data['email'][0]
		password = data['password'][0]

		db = VGDB()
		db.createVideogamesTable()
		user = db.getUserByEmail(email)
		if user == None:
			self.handle401()
		#elif bcrypt.hashpw(password,user['password']):
		elif bcrypt.verify(password, user['password']):
			self.session["userId"] = user['id']
			self.send_response(201)
			self.end_headers()
		else:
			self.handle401()
	
	def handleSessionDelete(self):
		self.session["userId"] = None
		self.send_response(200)
		self.end_headers
	def handleUserCreate(self):
		length = self.headers["Content-length"]
		body = self.rfile.read(int(length)).decode("utf-8")
		print("THE BODY: ", body,"\n")
		data = parse_qs(body)
		username = data["username"][0]
		email = data["email"][0]
		password = data["password"][0]
		db = VGDB()
		db.createUsersTable()
		user = db.getUserByEmail(email)
		if user != None:
			self.handle422()
			return
		hashedPassword = bcrypt.hash(password)
		db.createUser(username,email,hashedPassword)
		self.send_response(201)
		self.end_headers()
	def handleUserRetrieve(self):
		if "userId" not in self.session:
			self.handle401()
			return
		self.send_response(200)
		self.send_header("content-type","application/json")
		self.end_headers()
		db = VGDB()
		db.createUsersTable()
		user = db.getUser(self.session["userId"])
		print(user)
		self.wfile.write(bytes(json.dumps(user), "utf-8"))

	def handle401(self):
		self.send_response(401)
		self.send_header("content-type","text/html")
		self.end_headers()
		self.wfile.write(bytes("<h1>401 error.</h1>", "utf-8"))
	def send404(self):
		self.send_response(404)
		self.send_header("content-type","text/html")
		self.end_headers()
		self.wfile.write(bytes("<h1>404 error page not found.</h1>", "utf-8"))
	def handle422(self):
		self.send_response(422)
		self.send_header("content-type","text/html")
		self.end_headers()
		self.wfile.write(bytes("<h1>422 error.</h1>", "utf-8"))


def run():

	port = 8080
	if len(sys.argv) > 1:
		port = int(sys.argv[1])

	listen = ("0.0.0.0", port)
	server = HTTPServer(listen, Handler)

	print("Listening...")
	server.serve_forever()

run()
