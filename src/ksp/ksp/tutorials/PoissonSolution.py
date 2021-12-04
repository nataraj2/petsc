#!/usr/loca/bin/python
from numpy import *
from pylab import *
from matplotlib import *

data = loadtxt("pressure.txt");

m = max(data[:,0])+1;
n = max(data[:,1])+1

m = int(m)
n = int(n)

X = zeros([n, m]);
Y = zeros([n, m]);
pressure = zeros([n, m]);
exact = zeros([n, m]);

for q in arange(0,m*n,1):

	i = int(data[q,0])
	j = int(data[q,1])
	
	pressure[j, i] = data[q,4]
	x = data[q,2]
	y = data[q,3]
	X[j, i] = x
	Y[j, i] = y

	exact[j,i] = cos(2*pi*x)*cos(2*pi*y)

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

