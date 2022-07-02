#ifndef _COMMON_LOG_IMPL_H_
#define _COMMON_LOG_IMPL_H_

template <class T> void
Log::Dump(opnum_t from, T out)
{
    for (opnum_t i = std::max(from, start);
         i <= LastOpnum(); i++) {

        const LogEntry *entry = Find(i);
        ASSERT(entry != NULL);
        
        auto elem = out->Add();
        elem->set_view(entry->viewstamp.view);
        elem->set_opnum(entry->viewstamp.opnum);
        elem->set_state(entry->state);
        elem->set_hash(entry->hash);
        *(elem->mutable_request()) = entry->request;        
    }
}

template <class iter> void
Log::Install(iter start, iter end)
{
    // Find the first divergence in the log
    iter it = start;
    for (it = start; it != end; it++) {
        const LogEntry *oldEntry = Find(it->opnum());
        if (oldEntry == NULL) {
            break;
        }
        if (it->view() != oldEntry->viewstamp.view) {
            RemoveAfter(it->opnum());
            break;
        }
    }

    if (it == end) {
        // We didn't find a divergence. This means that the logs
        // should be identical. If the existing log is longer,
        // something is wrong.
//            it--;
//            ASSERT(it->opnum() == lastViewstamp.opnum);
//            ASSERT(it->view() == lastViewstamp.view);
//            ASSERT(Find(it->opnum()+1) == NULL);
    }

    // Install the new log entries
    for (; it != end; it++) {
        viewstamp_t vs = { it->view(), it->opnum() };
        Append(vs, it->request(), LOG_STATE_PREPARED);
    }
}

#endif  /* _COMMON_LOG_IMPL_H_ */
