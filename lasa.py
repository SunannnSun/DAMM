import pyLasaDataset as lasa
import numpy as np
from main import dpmm

# https://bitbucket.org/khansari/lasahandwritingdataset/src/master/DataSet/

sub_sample = 4
# [BendedLine, Leaf_1, Leaf_2, Snake， Trapezoid, Worm]
data = lasa.DataSet.Leaf_2
dt = data.dt
demos = data.demos # list of 7 Demo objects, each corresponding to a 
demo_0 = demos[0]
pos = demo_0.pos[:, ::sub_sample] # np.ndarray, shape: (2,2000)
vel = demo_0.vel[:, ::sub_sample] # np.ndarray, shape: (2,2000) 
Data = np.vstack((pos, vel))
for i in np.arange(1, len(demos)):
    pos = demos[i].pos[:, ::sub_sample]
    vel = demos[i].vel[:, ::sub_sample]
    Data = np.hstack((Data, np.vstack((pos, vel))))
DPMM = dpmm(Data)
DPMM.begin()
# assignArr = DPMM.assignment_array
# a =  np.where(assignArr==assignArr.max()-2)[0]
# b =  np.where(assignArr==assignArr.max()-3)[0]
# ab = a.tolist() + b.tolist()
# np.save('array.npy', ab)

# ab = np.load('array.npy')
# DPMM = dpmm(Data[:, ab])
# DPMM.begin()