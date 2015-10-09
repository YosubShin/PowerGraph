#! /usr/bin/python

from subprocess import STDOUT, check_call
import os
import subprocess

num_hosts = 16
home_path = '/home/yosub_shin_0'
power_graph_home = '%s/PowerGraph' % home_path
graph_analytics_bin_path = '%s/release/toolkits/graph_analytics' % power_graph_home
# num_iterations = [1, 5, 10, 20]
num_iterations = [5, 10, 20]
# algorithms = ['pagerank']
algorithms = ['sssp']

partitioning_strategies = ['grid', random', 'oblivious']
graph_path = '%s/graphs/livejournal/' % home_path

hostfile_path = '%s/machines' % home_path

for run in range(3):  # Do experiment X many times
    for num_iteration in num_iterations:
        lines = subprocess.check_output(['shuf', '-n', str(num_iteration), '%s/lab_data_lj' % home_path])
        lines = lines.split('\n')
        source_list = map(lambda l: '--source=' + l.split('\t')[0], filter(lambda x: len(x) > 0, lines))
        for algorithm in algorithms:
            algorithm_path = '%s/%s' % (graph_analytics_bin_path, algorithm)
            for partitioning_strategy in partitioning_strategies:
                print('algorithm: %s, partitioning_strategy: %s, num_iteration: %d' % (algorithm, partitioning_strategy, num_iteration))
                # check_call(['mpiexec', '-n', str(num_hosts), '-hostfile', hostfile_path, algorithm_path, '--graph=%s' % graph_path, '--format=snap', '--iterations=%d' % num_iteration, '--graph_opts=ingress=%s' % partitioning_strategy],
                #            stderr=STDOUT) 
                check_call(['mpiexec', '-n', str(num_hosts), '-hostfile', hostfile_path, algorithm_path, '--graph=%s' % graph_path, '--format=snap', '--graph_opts=ingress=%s' % partitioning_strategy] + source_list,
                           stderr=STDOUT) 
