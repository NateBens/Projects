import os, base64

class SessionStore:
		
	def __init__(self):
		self.sessions = {}
		return
	def generateSessionId(self):
		rnum = os.urandom(32)
		rstr = base64.b64encode(rnum).decode("utf-8")
		return rstr
	def createSession(self):
		sessionId = self.generateSessionId()
		self.sessions[sessionId] = {}
		return sessionId
	def getSession(self,sessionId):
		if sessionId in self.sessions:
			return self.sessions[sessionId]
		else:
			return None