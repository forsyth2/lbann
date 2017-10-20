import csv
import os
import re
import sys
import time
import unittest

def model_test(model_name):
  return '--model=../model_zoo/tests/model_%s.prototext' % model_name

def model(model_name):
  return '--model=../model_zoo/models/%s/model_%s.prototext' % (model_name, model_name)

def reader(reader_name):
  return '--reader=../model_zoo/data_readers/data_reader_%s.prototext' % reader_name

def optimizer(optimizer_name):
  return '--optimizer=../model_zoo/optimizers/opt_%s.prototext' % optimizer_name

def run_lbann(model_name, reader_name, optimizer_name, output_file_name, executable, num_nodes=1, timeout=60, num_processes=2):
  if model_name == 'mnist_distributed_io':
    m = model_test(model_name)
  else:
    m = model(model_name)
  r = reader(reader_name)
  o = optimizer(optimizer_name)
  # N => number of nodes                                                         
  # p => partition
  # t => timeout period, in minutes
  # n => number of processes to run. MPI Rank. How many models being made.
  # num-epochs => The number of epochs
  command = 'salloc -N %d -p pbatch -t %d srun -n %d %s %s %s %s --num_epochs=2 > %s' % (num_nodes, timeout, num_processes, executable, m, r, o, output_file_name)
  print('Began %s at ' % model_name + time.strftime('%H:%M:%S', time.localtime()))
  value = os.system(command)
  print('Ended %s at ' % model_name + time.strftime('%H:%M:%S', time.localtime()))
  if value != 0:
    raise Exception('Model %s crashed' % model_name)

def get_performance(output_file_name):
  output_file = open(output_file_name, 'r')
  performance_dict = {}
  for line in output_file:
    m = re.match('(Model [0-9]+) Epoch time: ([0-9]+(\.[0-9]+)*)s; Mean minibatch time: ([0-9]+(\.[0-9]+)*)s; Min: ([0-9]+(\.[0-9]+)*)s; Max: ([0-9]+(\.[0-9]+)*)s; Stdev: ([0-9]+(\.[0-9]+)*)s', line)
    if m:
      model = m.group(1)
      if model not in performance_dict.keys():
        performance_dict[model] = {
          'accuracies' : [],
          'epoch_times' : [],
          'mean_minibatch_times' : [],
          'mins' : [],
          'maxs' : [],
          'stdevs' : []
        }
      performance_dict[model]['epoch_times'].append(float(m.group(2)))
      performance_dict[model]['mean_minibatch_times'].append(float(m.group(4)))
      performance_dict[model]['mins'].append(float(m.group(6)))
      performance_dict[model]['maxs'].append(float(m.group(8)))
      performance_dict[model]['stdevs'].append(float(m.group(10)))
    a = re.match('(Model [0-9]+) @[0-9]+ testing steps external validation categorical accuracy: ([0-9]+(\.[0-9]+)*)', line)
    if a:
      model = a.group(1)
      if model not in performance_dict.keys():
        performance_dict[model] = {
          'accuracies' : [],
          'epoch_times' : [],
          'mean_minibatch_times' : [],
          'mins' : [],
          'maxs' : [],
          'stdevs' : []
        }
      performance_dict[model]['accuracies'].append(float(a.group(2)))
  output_file.close()
  return performance_dict


def csv_to_dict(csv_path):
  with open(csv_path, 'r') as csv_file:
    reader = csv.reader(csv_file, skipinitialspace=True)
    keys = reader.next()
    expected_times = {}
    for row in reader:
      model = row[0]
      expected_times[model] = dict(zip(keys[1:], map(float, row[1:])))
  return expected_times

def run_tests(performance, model_name):
  expected_times = csv_to_dict('integration_tests/performance_tests/expected_performance.csv')
  errors = []
  for model_num in performance.keys():
    p = performance[model_num]
    e = expected_times[model_name]
    for epoch_time in p['epoch_times']:
      if epoch_time > e['max_epoch_time']:
        errors.append('%.2f > %.2f %s %s max_epoch_time' % (epoch_time, e['max_epoch_time'], model_name, model_num))
    for mean_minibatch_time in p['mean_minibatch_times']:
      if mean_minibatch_time > e['max_mean_minibatch_time']:
        errors.append('%.2f > %.2f %s %s max_mean_minibatch_time' % (mean_minibatch_time, e['max_mean_minibatch_time'], model_name, model_num))
    for min_time in p['mins']:
      if min_time > e['max_min_time']:
        errors.append('%.2f > %.2f %s %s max_min_time' % (min_time, e['max_min_time'], model_name, model_num))
    for max_time in p['maxs']:
      if max_time > e['max_max_time']:
        errors.append('%.2f > %.2f %s %s max_max_time' % (max_time, e['max_max_time'], model_name, model_num))
    for stdev in p['stdevs']:
      if stdev > e['max_stdev']:
        errors.append('%.2f > %.2f %s %s max_stdev' % (stdev, e['max_stdev'], model_name, model_num))
    for accuracy in p['accuracies']:
      if accuracy < e['min_accuracy']:
        errors.append('%.2f < %.2f %s %s min_accuracy' % (accuracy, e['min_accuracy'], model_name, model_num))
  assert errors == []

def mnist_distributed_io_skeleton(executable):
  model_name = 'mnist_distributed_io'
  output_file_name = '%s_output.txt' % model_name
  run_lbann(model_name=model_name, reader_name='mnist', optimizer_name='adagrad', output_file_name=output_file_name, executable=executable)
  performance = get_performance(output_file_name)
  run_tests(performance, model_name)

def alexnet_skeleton(executable):
  model_name = 'alexnet'
  output_file_name = '%s_output.txt' % model_name
  run_lbann(model_name=model_name, reader_name='imagenet', optimizer_name='adagrad', output_file_name=output_file_name, executable=executable)
  performance = get_performance(output_file_name)
  run_tests(performance, model_name)

def resnet50_skeleton(executable):
  model_name = 'resnet50'
  output_file_name = '%s_output.txt' % model_name
  run_lbann(model_name=model_name, reader_name='imagenet', optimizer_name='adagrad', output_file_name=output_file_name, executable=executable)
  performance = get_performance(output_file_name)
  run_tests(performance, model_name)
