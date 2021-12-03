#!/usr/loca/bin/python
from numpy import *
from pylab import *
from matplotlib import *

data = loadtxt("pressure.txt");

m = max(data[:,0])+1;
n = max(data[:,1])+1

print m, n

X = zeros([n, m]);
Y = zeros([n, m]);
pressure = zeros([n, m]);
exact = zeros([n, m]);

for q in arange(0,size(data[:,0]),1):

	pressure[data[q,1], data[q,0]] = data[q,4]
	x = data[q,2]
	y = data[q,3]
	X[data[q,1], data[q,0]] = x
	Y[data[q,1], data[q,0]] = y

	exact[data[q,1],data[q,0]] = cos(2*pi*x)*cos(2*pi*y)

plt.figure(1)
plt.contourf(X,Y,pressure,20,cmap=cm.jet)
plt.colorbar()
plt.title("PetSc solution",fontsize=20)
plt.figure(2)
plt.contourf(X,Y,exact,20,cmap=cm.jet)
plt.colorbar()
plt.title("Exact solution",fontsize=20)


#plt.axis("equal")
plt.show()

