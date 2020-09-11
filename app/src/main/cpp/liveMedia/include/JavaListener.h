//
// Created by lt on 2020-06-16.
//

class JavaListener {
//定义类成员属性
public:
    JavaVM *vm;
    JNIEnv *env;
    jobject jobj;
    jmethodID jmethod;

public:
    //定义构造函数
    JavaListener(JavaVM *vm, JNIEnv *env, jobject jobj);

    //析构函数
    ~JavaListener();
    //定义类成员方法 onSuccess 在这个函数会负责去调用 Java 层的 onSuccess 函数
    void onSuccess(const char *msg);
};