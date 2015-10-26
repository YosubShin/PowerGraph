#! /usr/bin/python

from subprocess import STDOUT, check_call
import os
import subprocess

num_hosts = 16
home_path = '/home/yosub_shin_0'
power_graph_home = '%s/PowerGraph' % home_path
graph_analytics_bin_path = '%s/release/toolkits/graph_analytics' % power_graph_home
tolerances = [0.1, 0.01, 0.001, 0.0001]
algorithms = ['approximate_diameter']

partitioning_strategies = ['grid', 'random', 'oblivious']
graph_path = '%s/graphs/livejournal/' % home_path

hostfile_path = '%s/machines' % home_path

for run in range(3):  # Do experiment X many times
    for tolerance in tolerances:
        for algorithm in algorithms:
            algorithm_path = '%s/%s' % (graph_analytics_bin_path, algorithm)
            for partitioning_strategy in partitioning_strategies:
                print('algorithm: %s, partitioning_strategy: %s, tolerance: %f' % (algorithm, partitioning_strategy, tolerance))
                check_call(['mpiexec', '-n', str(num_hosts), '-hostfile', hostfile_path, algorithm_path, '--graph=%s' % graph_path, '--format=snap', '--graph_opts=ingress=%s' % partitioning_strategy, '--tol=%f' % tolerance],
                           stderr=STDOUT) 
