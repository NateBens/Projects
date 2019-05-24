import math
import random

alphabet70 = ".,?! \t\n\rabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
alphabet26 = "abcdefghijklmnopqrstuvwxyz"
stringa = "itisnotbecausethingsaredifficultthatwedonotnotdarebutbecausewedonotdarethingsaredifficultitisbettertobeunbornthanuntaughtforignoranceistherootofmisfortune"
stringb = "whataintnocountryiveeverheardtheyspeakenglishinwhatwhatenglishmotherfuckerdoyouspeakityesyesthenyouknowwhatimsayinyesdescribewhatmarcelluswallacelookslikewhatsaywhatagainsaywhatagainidareyouidoubledareyoumotherfuckersaywhatonemoregoddamntime"
def millerTest(n):
	b=random.randrange(2,n)
	t=n-1
	s=0
	#Find S and T
	while (t%2==0):
		t=t//2
		s+=1
	##
	t = int(t)
	if (pow(b,t,n)) == 1:
		return True
	for j in range(0,s):
		x= (2**j)*t
		if pow(b,x,n)==(n-1):
			return True
	return False
def isPrimeMonteCarlo(n):
	if n%2==0:
		return False
	for i in range(10):
		ok = millerTest(n)
		if not ok:
			return False
	return True
def toBase10(alphabet,s):
	value = 0 
	for char in s:
		if char in alphabet:
			pos = alphabet.find(char)
			value *= len(alphabet)
			value += pos
	return value

def fromBase10(alphabet,base10Num):
	newBaseMessage =''
	while base10Num >0 :
		base10Num,i = divmod(base10Num,len(alphabet))
		newBaseMessage = alphabet[i] + newBaseMessage
	return newBaseMessage


def extended_gcd(aa, bb):
    lastremainder, remainder = abs(aa), abs(bb)
    x, lastx, y, lasty = 0, 1, 1, 0
    while remainder:
        lastremainder, (quotient, remainder) = remainder, divmod(lastremainder, remainder)
        x, lastx = lastx - quotient*x, x
        y, lasty = lasty - quotient*y, y
    return lastremainder, lastx * (-1 if aa < 0 else 1), lasty * (-1 if bb < 0 else 1)
def modinv(a, m):
	g, x, y = extended_gcd(a, m)
	if g != 1:
		raise ValueError
	return x % m
class RSA:

	def __init__(self):
		return

	def generateKeys(self,s1,s2):
		#Convert long strings to base 10 numbers, treat as base 26
		p = toBase10(alphabet26,s1)
		q = toBase10(alphabet26,s2)
		#Must initially be longer than 200 digits
		if (len(str(p)) < 200) or (len(str(q)) < 200):
			print("Strings Not Sufficient length")
		#make them 200 digits
		p = p % (10**200)
		q = q % (10**200)
		#Make them odd
		if p % 2 == 0:
			p+=1
		if q % 2 == 0:
			q+=1
		#Add 2 until they are prime
		primep = isPrimeMonteCarlo(p)
		primeq = isPrimeMonteCarlo(q)
		while primep==False:
			p+=2
			primep = isPrimeMonteCarlo(p)
		while primeq==False:
			q+=2
			primeq = isPrimeMonteCarlo(q)
		#Calculate n and r 
		n = p*q
		r = (p-1)*(q-1)
		#Find e – a 398 digit number that is relatively prime with r
		e = r % (10**398)
		gcder = math.gcd(r,e)
		while gcder != 1:
			if e % 2 == 0:
				e+=1
				gcder = math.gcd(r,e)
			else:
				e+=2
				gcder = math.gcd(r,e)

		#Find d – the inverse of e mod r
		d = modinv(e,r)
		#Save n and e to a file called public.txt (write them as text, with 1 return after each number)
		public  = open("public.txt","w")
		public.write(str(n))
		public.write("\n")
		public.write(str(e))
		public.close()
		#Save n and d to a file called private.txt
		private  = open("private.txt","w")
		private.write(str(n))
		private.write("\n")
		private.write(str(d))
		private.close()
		#print(len(str(n)))
		#print(len(str(e)))
		#print(e)
		#print(len(str(d)))
		#print(d)
		#print(isPrimeMonteCarlo(p))

	def encrypt(self,infile,outfile):
		#Read in message to be encrypted
		fin = open(infile,"rb")
		PlainTextBinary = fin.read()
		PlainText = PlainTextBinary.decode("utf-8")
		fin.close()
		# read in n and e from public file
		fin = open("public.txt","r")
		ne = []
		for line in fin:
			ne.append(line)
		fin.close()
		ne = [x.strip() for x in ne]
		n = int(ne[0])			
		e = int(ne[1])
		#Calculate the number of characters per block of text
		charsPerBlock = math.log(n,70)
		charsPerBlock = int(charsPerBlock)
		#Calculate blocks needed 
		blocksNeeded = (len(PlainText)-1)//charsPerBlock + 1
		#Convert base 70 message to base 10 number/text
		#base10text = str(base10text)
		#Chop base 10 message into blocks
		print ("blocksNeeded: ",blocksNeeded)
		fout = open(outfile,"wb")
		for i in range(blocksNeeded):
			plainblock = PlainText[(i*charsPerBlock):(i+1)*charsPerBlock]
			base10 = toBase10(alphabet70,plainblock)
			encodedBlock = pow(base10,e,n)
			encodedbase70text = fromBase10(alphabet70,encodedBlock)
			encodedbase70text += '$'
			fout.write(encodedbase70text.encode("utf-8"))
		fout.close()
		#Encode each block using RSA Rules 
		
		#print(encodedBlocks)
		#print("-----")
		#print(blocksNeeded)
		#print("-----")
		#print(charsPerBlock)
		#print("-----")
		#print(blocks)
		#print(base10text)
		#print("-----")
		#print(n)
		#print("-----")
		#print(e)
	def decrypt(self,infile,outfile):
		fin = open(infile,'rb')
		EncryptedTextBinary = fin.read()
		EncryptedText = EncryptedTextBinary.decode("utf-8")
		fin.close()
		blocks = EncryptedText.split('$')
		#remove the 'b' that for some reason appears when reading in the binary file
		#blocks[0] = blocks[0][2:]
		# convert base 70 blocks to base 10 blocks
		#print("----")
		#print(len(blocks))
		#print("----")
		fin = open("private.txt","r")
		nd = []
		for line in fin:
			nd.append(line)
		fin.close()
		nd = [x.strip() for x in nd]
		n = int(nd[0])			
		d = int(nd[1])
		fout = open(outfile,"wb")
		for i in range(len(blocks)-1):
			base10block = toBase10(alphabet70,blocks[i])
			decryptedNumber = pow(base10block,d,n)
			decryptedText = fromBase10(alphabet70,decryptedNumber)
			fout.write(decryptedText.encode("utf-8"))
		fout.close()
		#print("----")
		#print(base10blocks)
		#print(len(str(base10blocks[0])))
		#print("----")
		#read in n and d
		#print(decryptedString)
		#print("----")
		
		
		
		#print(n)
		#print(d)

def main():
	rsa = RSA()
	#rsa.generateKeys(stringa,stringb)
	#rsa.encrypt("message.txt","encryptedMessage.txt")
	rsa.decrypt("NathanEncrypted.txt","decryptedMessage.txt")
	
main()
