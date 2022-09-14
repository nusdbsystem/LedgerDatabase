#include <numeric>
#include <future>
#include "tbb/concurrent_queue.h"
#include "tbb/concurrent_hash_map.h"
#include "distributed/store/common/truetime.h"
#include "distributed/store/common/frontend/client.h"
#include "distributed/store/strongstore/client.h"
#include "boost/algorithm/string.hpp"

using namespace std;

double theta;  // no use

int randconstcid;
int randconstiid;

enum Mode{
  MODE_UNKNOWN,
  MODE_TAPIR,
  MODE_WEAK,
  MODE_STRONG
};

struct Task {
  int op;
  std::vector<std::string> keys;
};

tbb::concurrent_queue<Task> task_queue;
bool running = true;

int nurand(int a, int x, int y, int constrand) {
  auto randa = rand()/a;
  auto randb = x + rand()/(y-x);
  return (((randa | randb) + constrand) % (y - x + 1)) + x;
}

std::vector<std::string> parseFields(const std::string& row) {
  std::vector<std::string> results;
  boost::split(results, row, [](char c){return c == ',';});
  return results;
}

std::string fieldsToString(const std::vector<std::string> fields) {
  std::string delimiter = ",";
  std::string result = std::accumulate(std::next(fields.begin()),
      fields.end(), fields[0],
      [&delimiter](std::string& a, const std::string& b) {
          return a + delimiter + b;
      });
  return result;
}

void genNewOrder(std::vector<std::string>& keys) {
  auto w_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/5));
  keys.emplace_back(w_id);
  auto d_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/10));
  keys.emplace_back(d_id);
  auto c_id = std::to_string(nurand(1023, 1, 3000, randconstcid));
  keys.emplace_back(c_id);
  auto num_items = 1 + rand()/((RAND_MAX + 1u)/5);
  for (size_t i = 0; i < num_items; ++i) {
    auto i_id = std::to_string(nurand(8191, 1, 100000, randconstiid));
    keys.emplace_back(i_id);
    auto s_w_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/5));
    keys.emplace_back(s_w_id);
    auto quantity = std::to_string(5 + rand()/((RAND_MAX + 1u)/10));
    keys.emplace_back(quantity);
  }
}

void genPayment(std::vector<std::string>& keys) {
  auto w_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/5));
  keys.emplace_back(w_id);
  auto d_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/10));
  keys.emplace_back(d_id);
  auto c_id = std::to_string(nurand(1023, 1, 3000, randconstcid));
  keys.emplace_back(c_id);
  auto payment = std::to_string(double(100 + rand()/((RAND_MAX + 1u)/500000))/100);
  keys.emplace_back(payment);
}

void genDelivery(std::vector<std::string>& keys) {
  auto w_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/5));
  keys.emplace_back(w_id);
  auto o_carrier_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/10));
  keys.emplace_back(o_carrier_id);
}

void genOrderStatus(std::vector<std::string>& keys) {
  auto w_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/5));
  keys.emplace_back(w_id);
  auto d_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/10));
  keys.emplace_back(d_id);
  auto c_id = std::to_string(nurand(1023, 1, 3000, randconstcid));
  keys.emplace_back(c_id);
}

void genStockLevel(std::vector<std::string>& keys) {
  auto w_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/5));
  keys.emplace_back(w_id);
  auto d_id = std::to_string(1 + rand()/((RAND_MAX + 1u)/10));
  keys.emplace_back(d_id);
  auto threshold = std::to_string(10 + rand()/((RAND_MAX + 1u)/10));
  keys.emplace_back(threshold);
}

void execNewOrder(std::vector<std::string>& keys, strongstore::Client* client) {
  auto w_id = keys[0];
  auto d_id = keys[1];
  auto c_id = keys[2];
  client->BufferKey("w_tax_" + w_id);
  client->BufferKey("d_tax_" + d_id + "_" + w_id);
  client->BufferKey("c_discount_" + c_id + "_" + d_id + "_" + w_id);

  std::vector<std::string> i_ids;
  std::vector<std::string> s_w_ids;
  std::vector<uint32_t> quantities;
  size_t idx = 3;
  while (idx < keys.size()) {
    auto i_id = keys[idx];
    client->BufferKey("i_" + i_id);
    i_ids.emplace_back(i_id);
    ++idx;

    auto s_w_id = keys[idx];
    client->BufferKey("s_" + i_id + "_" + s_w_id);
    s_w_ids.emplace_back(s_w_id);
    ++idx;

    auto quantity = std::stoi(keys[idx]);
    quantities.emplace_back(quantity);
    ++idx;
  }

  std::map<string, string> values;
  client->BatchGet(values);

  uint64_t order_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  auto warehouse = values["w_tax_" + w_id];
  auto district = parseFields(values["d_tax_" + d_id + "_" + w_id]);
  auto customer = values["c_discount_" + c_id + "_" + d_id + "_" + w_id];
  auto w_tax = std::stod(warehouse);
  auto d_tax = std::stod(district[0]);
  auto c_discount = std::stod(customer);
  size_t o_id = std::stol(district[1]);
  int o_all_local = 1;
  for (size_t i = 0; i < i_ids.size(); ++i) {
    auto item = parseFields(values["i_" + i_ids[i]]);
    auto stock = parseFields(values["s_" + i_ids[i] + "_" + s_w_ids[i]]);
    if (stock[1].compare(w_id) != 0) o_all_local = 0;
    auto price = std::stod(item[3]);
    auto dist_info = stock[2 + std::stod(d_id)];
    auto s_quantity = std::stol(stock[2]);
    if (s_quantity > quantities[i]) {
      s_quantity -= quantities[i];
    } else {
      s_quantity = s_quantity - quantities[i] + 100;
    }
    stock[2] = std::to_string(s_quantity);
    client->Put("s_" + stock[0] + "_" + stock[1], fieldsToString(stock));
    
    auto ol_amount =
        quantities[i] * price * (1 + w_tax + d_tax) * (1 - c_discount);
    auto ol_key = "ol_" + std::to_string(o_id) + "_" + d_id + "_" + w_id +
                  "_" + std::to_string(i+1);
    std::string ol_val = std::to_string(o_id) + "," + 
                         d_id + "," +
                         w_id + "," +
                         std::to_string(i + 1) + "," +
                         i_ids[i] + "," +
                         s_w_ids[i] + ",," +
                         std::to_string(quantities[i]) + "," +
                         std::to_string(ol_amount) + "," +
                         dist_info;
    client->Put(ol_key, ol_val);
  }
  auto order_key = "o_" + std::to_string(o_id) + "_" + d_id + "_" + w_id;
  auto order_val = std::to_string(o_id) + "," + 
                   d_id + "," + 
                   w_id + "," + 
                   c_id + "," + 
                   std::to_string(order_time) + ",," +  // carrier_id empty
                   std::to_string(i_ids.size()) + "," + 
                   std::to_string(o_all_local);
  client->Put(order_key, order_val);
  client->Put("c_last_order_" + c_id + "_" + d_id + "_" + w_id,
      std::to_string(o_id));
  district[1] = std::to_string(o_id + 1);
  client->Put("d_tax_" + d_id + "_" + w_id, fieldsToString(district));
}

void execPayment(std::vector<std::string>& keys, strongstore::Client* client) {
  auto w_id = keys[0];
  auto d_id = keys[1];
  auto c_id = keys[2];
  auto payment = std::stod(keys[3]);
  client->BufferKey("w_" + w_id);
  client->BufferKey("d_" + d_id + "_" + w_id);
  client->BufferKey("c_" + c_id + "_" + d_id + "_" + w_id);
  std::map<string, string> values;
  client->BatchGet(values);

  auto warehouse = parseFields(values["w_" + w_id]);
  auto district = parseFields(values["d_" + d_id + "_" + w_id]);
  auto customer = parseFields(values["c_" + c_id + "_" + d_id + "_" + w_id]);

  auto w_ytd = std::stod(warehouse[7]);
  auto d_ytd = std::stod(district[8]);
  auto c_balance = std::stod(customer[15]);
  auto c_ytd_payment = std::stod(customer[16]);
  auto c_payment_cnt = std::stod(customer[17]);

  warehouse[7] = std::to_string(w_ytd + payment);
  district[8] = std::to_string(d_ytd + payment);
  customer[15] = std::to_string(c_balance - payment);
  customer[16] = std::to_string(c_ytd_payment + payment);
  customer[17] = std::to_string(c_payment_cnt + 1);
  
  client->Put("w_" + w_id, fieldsToString(warehouse));
  client->Put("d_" + d_id + "_" + w_id, fieldsToString(district));
  client->Put("c_" + c_id + "_" + d_id + "_" + w_id, fieldsToString(customer));
  uint64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  client->Put("h_" + c_id + "_" + d_id + "_" + w_id + "_" +
      std::to_string(time), keys[3]);
}

void execOrderStatus(std::vector<std::string>& keys, strongstore::Client* client) {
  auto w_id = keys[0];
  auto d_id = keys[1];
  auto c_id = keys[2];
  client->BufferKey("w_" + w_id);
  client->BufferKey("d_" + d_id + "_" + w_id);
  client->BufferKey("c_" + c_id + "_" + d_id + "_" + w_id);
  client->BufferKey("c_last_order_" + c_id + "_" + d_id + "_" + w_id);
  std::map<string, string> values;
  client->BatchGet(values);

  auto warehouse = parseFields(values["w_" + w_id]);
  auto district = parseFields(values["d_" + d_id + "_" + w_id]);
  auto customer = parseFields(values["c_" + c_id + "_" + d_id + "_" + w_id]);
  auto o_id = values["c_last_order_" + c_id + "_" + d_id + "_" + w_id];
  if (o_id.compare("") == 0) {
    return;
  }

  client->BufferKey("o_" + o_id + "_" + d_id + "_" + w_id);
  client->BatchGet(values);

  auto order = parseFields(values["o_" + o_id + "_" + d_id + "_" + w_id]);
  auto ol_cnt = std::stoi(order[6]);
  for (int i = 1; i <= ol_cnt; ++i) {
    client->BufferKey("ol_" + o_id + "_" + d_id + "_" + w_id + "_" +
        std::to_string(i));
  }
  client->BatchGet(values);
}

void execDelivery(std::vector<std::string>& keys, strongstore::Client* client) {
  auto w_id = keys[0];
  auto carrier_id = keys[1];

  for (size_t i = 0; i < 10; ++i) {
    client->BufferKey("d_tax_" + std::to_string(i+1) + "_" + w_id);
    client->BufferKey("next_d_id_" + std::to_string(i+1) + "_" + w_id);
  }
  std::map<string, string> values;
  client->BatchGet(values);

  uint64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

  std::vector<std::string> o_ids;
  for (size_t i = 0; i < 10; ++i) {
    auto district = parseFields(values["d_tax_" + std::to_string(i+1) + "_" + w_id]);
    size_t next_o_id = std::stol(district[1]);
    size_t d_id = 1;
    std::string next_d_id_key = "next_d_id_" + std::to_string(i+1) + "_" + w_id;
    if (values.find(next_d_id_key) != values.end() && values[next_d_id_key].size() > 0) {
      d_id = std::stol(values["next_d_id_" + std::to_string(i+1) + "_" + w_id]);
    }
    if (d_id >= next_o_id) {
      o_ids.push_back("");
      continue;
    }
    std::string o_id = std::to_string(d_id);
    o_ids.push_back(o_id);
    ++d_id;
    client->Put("next_d_id_" + std::to_string(i+1) + "_" + w_id, std::to_string(d_id));
    client->BufferKey("o_" + o_id + "_" + std::to_string(i+1) + "_" + w_id);
  }
  client->BatchGet(values);
  std::vector<std::string> c_ids;
  std::vector<size_t> ol_cnts;
  for (size_t i = 0; i < 10; ++i) {
    if (o_ids[i].size() == 0) {
      ol_cnts.push_back(0);
      c_ids.push_back("0");
      continue;
    }
    auto order = parseFields(values["o_" + o_ids[i] + "_" + std::to_string(i+1) + "_" + w_id]);
    auto ol_cnt = std::stoi(order[6]);
    ol_cnts.push_back(ol_cnt);
    order[5] = carrier_id;
    client->Put("o_" + order[0] + "_" + order[1] + "_" + order[2], fieldsToString(order));
    for (size_t j = 1; j <= ol_cnt; ++j) {
      client->BufferKey("ol_" + o_ids[i] + "_" + std::to_string(i+1) + "_" +
          w_id + "_" + std::to_string(j));
    }
    auto c_id = order[3];
    c_ids.push_back(c_id);
    client->BufferKey("c_" + c_id + "_" + std::to_string(i+1) + "_" + w_id);
  }
  client->BatchGet(values);

  for (size_t i = 0; i < 10; ++i) {
    if (o_ids[i].size() == 0) continue;
    double total = 0;
    for (size_t j = 1; j <= ol_cnts[i]; ++j) {
      auto orderline = parseFields(values["ol_" + o_ids[i] + "_" + std::to_string(i+1) + "_" +
          w_id + "_" + std::to_string(j)]);
      orderline[6] = std::to_string(time);
      client->Put("ol_" + o_ids[i] + "_" + std::to_string(i+1) + "_" +
          w_id + "_" + std::to_string(j), fieldsToString(orderline));
      auto ol_amount = std::stod(orderline[8]);
      total += ol_amount;
    }
    auto customer = parseFields(values["c_" + c_ids[i] + "_" + std::to_string(i+1) + "_" + w_id]);
    customer[15] = std::to_string(std::stod(customer[15]) + total);
    customer[18] = std::to_string(std::stod(customer[18]) + 1);
    client->Put("c_" + c_ids[i] + "_" + std::to_string(i+1) + "_" + w_id,
        fieldsToString(customer));
  }
}

void execStockLevel(std::vector<std::string>& keys, strongstore::Client* client) {
  auto w_id = keys[0];
  auto d_id = keys[1];
  auto threshold = std::stol(keys[2]);

  client->BufferKey("d_tax_" + d_id + "_" + w_id);
  std::map<string, string> values;
  client->BatchGet(values);

  auto district = parseFields(values["d_tax_" + d_id + "_" + w_id]);
  auto o_id = std::stol(district[1]);

  for (auto i = o_id - 20; i < o_id; ++i) {
    client->BufferKey("o_" + std::to_string(i) + "_" + d_id+ "_" + w_id);
  }
  client->BatchGet(values);

  std::vector<size_t> ol_cnts;
  for (auto i = o_id - 20; i < o_id; ++i) {
    std::string order_key = "o_" + std::to_string(i) + "_" + d_id+ "_" + w_id;
    if (values[order_key].size() == 0) {
      client->Abort();
      return;
    }
    auto order = parseFields(values[order_key]);
    size_t ol_cnt = std::stoi(order[6]);
    ol_cnts.push_back(ol_cnt);
    for (auto j = 0; j < ol_cnt; ++j) {
      client->BufferKey("ol_" + std::to_string(i) + "_" +
          d_id + "_" + w_id + "_" + std::to_string(j+1));
    }
  }
  client->BatchGet(values);

  size_t ol_cnt_idx = 0;
  std::vector<std::string> i_ids;
  for (auto i = o_id - 20; i < o_id; ++i) {
    for (auto j = 0; j < ol_cnts[ol_cnt_idx]; ++j) {
      std::string ol_key = "ol_" + std::to_string(i) + "_" + d_id + "_" +
          w_id + "_" + std::to_string(j+1);
      if (values[ol_key].size() == 0) {
        client->Abort();
        return;
      }
      auto orderline = parseFields(values[ol_key]);
      auto i_id = orderline[4];
      i_ids.push_back(i_id);
      auto s_w_id = orderline[5];
      if (s_w_id.compare(w_id) == 0) {
        client->BufferKey("s_" + i_id + "_" + w_id);
      }
    }
    ol_cnt_idx++;
  }
  client->BatchGet(values);

  size_t i_id_idx = 0;
  ol_cnt_idx = 0;
  std::set<std::string> result;
  for (auto i = o_id - 20; i < o_id; ++i) {
    for (auto i_id : i_ids) {
      auto stockstr = values["s_" + i_id + "_" + w_id];
      if (stockstr.size() > 0) {
        auto stock = parseFields(stockstr);
        size_t quantity = std::stol(stock[2]);
        if (quantity < threshold) {
          result.emplace(stock[0]);
        }
      }
    }
    ol_cnt_idx++;
  }
}

void millisleep(size_t t) {
  timespec req;
  req.tv_sec = (int) t/1000; 
  req.tv_nsec = (int64_t)((t%1000)*1e6); 
  nanosleep(&req, NULL); 
}

int taskGenerator(int clientid, int tid, int timeout) {
  timeval t0;
  gettimeofday(&t0, NULL);
  srand(t0.tv_sec*10000000000 + t0.tv_usec*10000 + clientid*10 + tid);
  randconstcid = rand()/1023;
  randconstiid = rand()/8191;
  while (true) {
    millisleep(timeout);
    if (!running) {
      std::cout << "task gen thread terminated" << std::endl;
      return 0;
    }

    auto ratio = rand()/((RAND_MAX + 1u)/100);
    Task task;
    if (ratio < 44) {
      task.op = 0;
      genNewOrder(task.keys);
    } else if (ratio < 88) {
      task.op = 1;
      genPayment(task.keys);
    } else if (ratio < 92) {
      task.op = 2;
      genOrderStatus(task.keys);
    } else if (ratio < 96) {
      task.op = 3;
      genDelivery(task.keys);
    } else {
      task.op = 4;
      genStockLevel(task.keys);
    }
    task_queue.push(task);
  }
  return 0;
}

int
txnThread(Mode mode, strongstore::Mode strongmode, const char *configPath,
  int nShards, int idx, int closestReplica, int skew, int error, int tLen,
  int wPer, int duration) {
  strongstore::Client *client = new strongstore::Client(strongmode, configPath,
        nShards, closestReplica, TrueTime(skew, error));

  // Read in the keys from a file.
  struct timeval t0, t1, t2;
  int nTransactions = 0;
  gettimeofday(&t0, NULL);

  while (1) {
    Task task;
    while (!task_queue.try_pop(task));

    bool status = true;
    if (task.op == 0) {
      std::cout << "New Order" << std::endl;
      client->Begin();
      gettimeofday(&t1, NULL);
      execNewOrder(task.keys, client);
      status = client->Commit();
      gettimeofday(&t2, NULL);
    } else if (task.op == 1) {
      std::cout << "Payment" << std::endl;
      client->Begin();
      gettimeofday(&t1, NULL);
      execPayment(task.keys, client);
      status = client->Commit();
      gettimeofday(&t2, NULL);
    } else if (task.op == 2) {
      std::cout << "Order Status" << std::endl;
      client->Begin();
      gettimeofday(&t1, NULL);
      execOrderStatus(task.keys, client);
      gettimeofday(&t2, NULL);
    } else if (task.op == 3) {
      std::cout << "Delivery" << std::endl;
      client->Begin();
      gettimeofday(&t1, NULL);
      execDelivery(task.keys, client);
      status = client->Commit();
      gettimeofday(&t2, NULL);
    } else {
      std::cout << "Stock Level" << std::endl;
      client->Begin();
      gettimeofday(&t1, NULL);
      execStockLevel(task.keys, client);
      gettimeofday(&t2, NULL);
    }

    long latency = (t2.tv_sec - t1.tv_sec)*1000000 +
                   (t2.tv_usec - t1.tv_usec);

    nTransactions++;

    fprintf(stderr, "%d %ld.%06ld %ld.%06ld %ld %d %d\n", nTransactions,
        t1.tv_sec, t1.tv_usec, t2.tv_sec, t2.tv_usec, latency, status?1:0, task.op);


    gettimeofday(&t1, NULL);
    if (((t1.tv_sec-t0.tv_sec)*1000000 + (t1.tv_usec-t0.tv_usec)) >
        duration*1000000) {
      running = false; 
      break;
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  const char *configPath = NULL;
  int duration = 10;
  int nShards = 1;
  int tLen = 10;
  int wPer = 50; // Out of 100
  int closestReplica = -1; // Closest replica id.
  int skew = 0; // difference between real clock and TrueTime
  int error = 0; // error bars
  int idx = 0;
  int txn_rate = 0;

  Mode mode = MODE_UNKNOWN;
  
  // Mode for strongstore.
  strongstore::Mode strongmode = strongstore::MODE_UNKNOWN;

  int opt;
  while ((opt = getopt(argc, argv, "c:d:N:l:w:k:f:m:e:s:z:r:i:x:")) != -1) {
    switch (opt) {
    case 'c': // Configuration path
    { 
      configPath = optarg;
      break;
    }

    case 'f': // Generated keys path
    { 
      break;
    }

    case 'N': // Number of shards.
    { 
      char *strtolPtr;
      nShards = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (nShards <= 0)) {
        fprintf(stderr, "option -n requires a numeric arg\n");
      }
      break;
    }

    case 'd': // Duration in seconds to run.
    { 
      char *strtolPtr;
      duration = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (duration <= 0)) {
        fprintf(stderr, "option -n requires a numeric arg\n");
      }
      break;
    }

    case 'l': // Length of each transaction (deterministic!)
    {
      char *strtolPtr;
      tLen = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (tLen <= 0)) {
        fprintf(stderr, "option -l requires a numeric arg\n");
      }
      break;
    }

    case 'w': // Percentage of writes (out of 100)
    {
      char *strtolPtr;
      wPer = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (wPer < 0 || wPer > 100)) {
        fprintf(stderr, "option -w requires a arg b/w 0-100\n");
      }
      break;
    }

    case 'k': // Number of keys to operate on.
    {
      break;
    }

    case 's': // Simulated clock skew.
    {
      char *strtolPtr;
      skew = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') || (skew < 0))
      {
        fprintf(stderr,
            "option -s requires a numeric arg\n");
      }
      break;
    }

    case 'e': // Simulated clock error.
    {
      char *strtolPtr;
      error = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') || (error < 0))
      {
        fprintf(stderr,
            "option -e requires a numeric arg\n");
      }
      break;
    }

    case 'z': // Zipf coefficient for key selection.
    {
      char *strtolPtr;
      theta = strtod(optarg, &strtolPtr);
      if ((*optarg == '\0') || (*strtolPtr != '\0'))
      {
        fprintf(stderr,
            "option -z requires a numeric arg\n");
      }
      break;
    }

    case 'r': // Preferred closest replica.
    {
      char *strtolPtr;
      closestReplica = strtod(optarg, &strtolPtr);
      if ((*optarg == '\0') || (*strtolPtr != '\0'))
      {
        fprintf(stderr,
            "option -r requires a numeric arg\n");
      }
      break;
    }

    case 'm': // Mode to run in [occ/lock/...]
    {
      if (strcasecmp(optarg, "txn-l") == 0) {
        mode = MODE_TAPIR;
      } else if (strcasecmp(optarg, "txn-s") == 0) {
        mode = MODE_TAPIR;
      } else if (strcasecmp(optarg, "qw") == 0) {
        mode = MODE_WEAK;
      } else if (strcasecmp(optarg, "occ") == 0) {
        mode = MODE_STRONG;
        strongmode = strongstore::MODE_OCC;
      } else if (strcasecmp(optarg, "lock") == 0) {
        mode = MODE_STRONG;
        strongmode = strongstore::MODE_LOCK;
      } else if (strcasecmp(optarg, "span-occ") == 0) {
        mode = MODE_STRONG;
        strongmode = strongstore::MODE_SPAN_OCC;
      } else if (strcasecmp(optarg, "span-lock") == 0) {
        mode = MODE_STRONG;
        strongmode = strongstore::MODE_SPAN_LOCK;
      } else {
        fprintf(stderr, "unknown mode '%s'\n", optarg);
        exit(0);
      }
      break;
    }

    case 'i': // index of client
    {
      char *strtolPtr;
      idx = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (idx < 0)) {
        fprintf(stderr, "option -i requires a numeric arg\n");
      }
      break; 
    }

    case 'x':  // txn rate
    {
      char *strtolPtr;
      txn_rate = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') ||
        (txn_rate < 0)) {
        fprintf(stderr, "option -x requires a numeric arg\n");
      }
      break; 
    }

    default:
      fprintf(stderr, "Unknown argument %s\n", argv[optind]);
      break;
    }
  }

  std::vector<std::future<int>> actual_ops;
  int numThread = 10;
  int interval = 1000 / txn_rate;
  for (int i = 0; i < numThread; ++i) {
    actual_ops.emplace_back(std::async(std::launch::async, taskGenerator, idx, i, interval));
  }
  actual_ops.emplace_back(std::async(std::launch::async, txnThread, mode, strongmode,
      configPath, nShards, idx, closestReplica, skew, error, tLen, wPer, duration));
  // vector<thread> thread_pool;                         
  // thread_pool.reserve(2);
  // thread_pool.emplace_back(txnThread, mode, strongmode);
  // thread_pool.emplace_back(verifyThread);
  // txnThread(mode, strongmode, configPath, nShards, idx, closestReplica, skew,
  //     error, tLen, wPer, duration);
}
