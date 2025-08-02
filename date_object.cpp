
// Only keep runtime call to get current time
extern "C" {
    int64_t __runtime_time_now_millis();
}
