# BlueWaters Deploy Manual

```bash
# From Login Node
qsub -I -l gres=ccm -l nodes=2:ppn=32:xe -l walltime=02:30:00
module add ccm
ccmlogin

# We are in a Compute Node.
module swap PrgEnv-cray PrgEnv-gnu
module load cmake
module swap gcc/4.8.2 gcc/4.9.3

export CC=/opt/gcc/4.9.3/bin/gcc
export CXX=/opt/gcc/4.9.3/bin/g++

export PROJECT_HOME=/projects/sciteam/jsb/shin1
cat $HOME/.crayccm/ccm_nodelist.$PBS_JOBID | sort -u > $PROJECT_HOME/machines


# Build OpenMPI
cd $PROJECT_HOME
mkdir openmpi
cd openmpi
wget http://www.open-mpi.org/software/ompi/v1.8/downloads/openmpi-1.8.4.tar.gz
tar zxvf openmpi-1.8.4.tar.gz
cd openmpi-1.8.4
./configure --prefix=$PROJECT_HOME/openmpi --enable-orterun-prefix-by-default --enable-mca-no-build=plm-tm,ras-tm
make install -j16

export PATH=$PATH:$PROJECT_HOME/openmpi/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PROJECT_HOME/openmpi/lib


# Build PowerGraph
cd $PROJECT_HOME
git clone https://github.com/YosubShin/PowerGraph
cd PowerGraph

./configure --no_jvm

cd release/toolkits/graph_analytics
make pagerank -j16

# Execute PowerGraph with PageRank
$PROJECT_HOME/openmpi/bin/mpiexec -n 2 -hostfile $PROJECT_HOME/machines $PROJECT_HOME/PowerGraph/release/toolkits/graph_analytics/pagerank --graph=$PROJECT_HOME/graphs/ukweb/ --format=snap --iterations=10 --graph_opts="ingress=random"
```
