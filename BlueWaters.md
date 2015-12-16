# BlueWaters Deploy Manual

```bash
# From Login Node
qsub -I -l gres=ccm -l nodes=64:ppn=32:xe -l walltime=02:00:00
module add ccm
ccmlogin

# We are in a Compute Node.
module swap PrgEnv-cray PrgEnv-gnu
module load cmake
module swap gcc/4.8.2 gcc/4.9.3
module load rca

export CC=/opt/gcc/4.9.3/bin/gcc
export CXX=/opt/gcc/4.9.3/bin/g++

# Change directory to work directory. If you don't have your own work directory, create one as /projects/sciteam/jsb/<your_user_name>
export PROJECT_HOME=/projects/sciteam/jsb/$USER
cd $PROJECT_HOME/PowerGraph/release/toolkits/graph_analytics


# Create a nodes list file
cat $HOME/.crayccm/ccm_nodelist.* | sort -u > $PROJECT_HOME/machines


# Build PowerGraph
cd $PROJECT_HOME
git clone https://github.com/YosubShin/PowerGraph
cd PowerGraph

./configure --no_jvm --no_mpi

cd release/toolkits/graph_analytics
make approximate_diameter -j16

# Use a synthetic power law graph
# For Approximate Diameter
python $PROJECT_HOME/PowerGraph/rpcexec.py -n 64 -f $PROJECT_HOME/machines $PROJECT_HOME/PowerGraph/release/toolkits/graph_analytics/approximate_diameter --powerlaw=5000000 --alpha=2.1 --indegree=1 --tol=10.0 --graph_opts="ingress=oblivious"

# For PageRank
python $PROJECT_HOME/PowerGraph/rpcexec.py -n 64 -t 0.1 -f $PROJECT_HOME/machines $PROJECT_HOME/PowerGraph/release/toolkits/graph_analytics/pagerank --powerlaw=50 --alpha=2.1 --indegree=1 --data_size=128 --graph_opts="ingress=oblivious"

# Or use a real graph file
python $PROJECT_HOME/PowerGraph/rpcexec.py -n 2 -f $PROJECT_HOME/machines $PROJECT_HOME/PowerGraph/release/toolkits/graph_analytics/pagerank --graph=$PROJECT_HOME/graphs/livejournal/  --format=snap --iterations=2 --graph_opts="ingress=random"
```
