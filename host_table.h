struct host_record {
    uint32_t gid;
    uint32_t id;
    int ip_count;
    uint32_t ip_list[MAX_NETWORK_SIZE];
    uint32_t ip_mask_list[MAX_NETWORK_SIZE];
    UT_hash_handle hh_gid;
    UT_hash_handle hh_id;
};
struct host_record *new_host_record(uint32_t id, uint32_t gid) {
    struct host_record *rec = malloc(sizeof(struct host_record));
    memset(rec, 0, sizeof(struct host_record));
    rec->gid = gid;
    rec->id = id;
    return rec;
}

static inline void add_host_record(struct host_record *rec) {
    HASH_ADD(hh_id, host_by_id, id, sizeof(int), rec);
    HASH_ADD(hh_gid, host_by_gid, gid, sizeof(int), rec);
}
static inline int add_host(uint32_t gid) {
    int old_net_size = net_size;
    struct host_record *rec = new_host_record(gid, net_size++);
    add_host_record(rec);
    return old_net_size;
}
static inline int query_id(uint32_t gid) {
    struct host_record *rec;
    HASH_FIND(hh_gid, host_by_gid, &gid, sizeof(int), rec);
    if (rec)
        return rec->id;
    else
        return -1;
}
static inline uint32_t query_gid(int id) {
    struct host_record *rec;
    HASH_FIND(hh_id, host_by_id, &id, sizeof(int), rec);
    if (rec)
        return rec->gid;
    else
        return -1;
}

struct ip_record {
    uint32_t gid;
    uint32_t ip;
    UT_hash_handle hh_ip;
};
struct ip_record *new_ip_record(uint32_t ip, uint32_t gid) {
    struct ip_record *rec = malloc(sizeof(struct ip_record));
    memset(rec, 0, sizeof(struct ip_record));
    rec->gid = gid;
    rec->ip = ip;
    return rec;
}
static inline void add_ip_record(struct ip_record *rec) {
    HASH_ADD(hh_ip, ip2gid, ip, sizeof(int), rec);
}
static inline uint32_t query_gid_by_ip(uint32_t ip) {
    struct ip_record *rec;
    HASH_FIND(hh_ip, ip2gid, &ip, sizeof(int), rec);
    if (rec)
        return rec->gid;
    else
        return -1;
}