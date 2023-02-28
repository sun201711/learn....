#include<iostream>
#include<vector>
#include<queue>
#include<future>
#include<atomic>
#include<mutex>

using namespace std;

#define ThreadPool_MAX_NUM 15

class MyThreadPool{
    vector<thread> threadpool;
    using Task = function<void()>;
    queue<Task> tasks;
    mutex _lock;
    condition_variable task_cv;

    unsigned short _ThreadPoolNum;
    atomic<bool> _run { true };  //线程池是否在运行
    atomic<int> _idleThreadNum {0};

public:

    MyThreadPool(unsigned short ThreadPoolNum = 4){
        _ThreadPoolNum = ThreadPoolNum;
        addThread(ThreadPoolNum);

    }
    ~MyThreadPool(){
        _run = false;
        task_cv.notify_all();
        for (thread& t:threadpool){
            if (t.joinable()){
                t.join();
            }
        }

    }
    int getIdelThreadNum(){
        return _idleThreadNum;
    }
    int getThreadPoolSize(){
        return threadpool.size();
    }

    // 提交一个任务
	// 调用.get()获取返回值会等待任务执行完,获取返回值
	// 有两种方法可以实现调用类成员，
	// 一种是使用   bind： .commit(std::bind(&Dog::sayHello, &dog));
	// 一种是用   mem_fn： .commit(std::mem_fn(&Dog::sayHello), this)
    template<class F, class ...Args>
    auto commit(F&& f, Args&& ...args)->future<decltype(f(args...))>{
        if (!_run){
            throw runtime_error("commit on ThreadPool is stopped.");
        }
        //将任务函数打包
        using tasktype = decltype(f(args...));
        auto task = make_shared<packaged_task<tasktype()>>
                    (bind(forward<F>(f), forward<Args>(args)...));
        future<tasktype> future = task->get_future();
        
        {
            lock_guard<mutex> ul(_lock);
            tasks.emplace([task](){
                (*task)();
            });

        }
        task_cv.notify_one();
        return future;
    }

    // 提交一个无参任务, 且无返回值
    template<class F>
    void commit2(F&& f){
        if (!_run){
            throw runtime_error("commit on ThreadPool is stopped.");
        }
        lock_guard<mutex> ul(_lock);
        tasks.emplace(forward<F>(f));
        task_cv.notify_one();
    }


private:
    void addThread(unsigned short ThreadPoolNum){
        for(;threadpool.size()<ThreadPool_MAX_NUM && ThreadPoolNum>0; ThreadPoolNum--){
            //创建一个线程
            threadpool.emplace_back([this]{
                while(true){ //线程循环获取task工作
                    Task task;
                    unique_lock<mutex> ul(_lock);
                    task_cv.wait(ul, [this]
                          {return !_run || !tasks.empty();});
                    if(!_run && tasks.empty()){
                        return;
                    }
                    _idleThreadNum--;
                    task = move(tasks.front());
                    tasks.pop();
                    
                    task();

                    unique_lock<mutex> ul(_lock);
                    _idleThreadNum++;

                }
            });

            //空闲线程++
            unique_lock<mutex> ul(_lock);
            _idleThreadNum++;
        }

    }
    
    

};
