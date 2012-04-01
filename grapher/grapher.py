import matplotlib.pyplot as plt

plt.plot_date([2001,2001.5,2002,2003,2004,2005],[1,2,3,4,5,6],xdate=True)
plt.ylabel('some numbers')
plt.savefig("fig1")
plt.show()