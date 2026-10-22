// Reporting an error must not collect more rejections from its accessors.
let calls = 0;
const reason = {
    toString() {
        if (++calls > 1) print('repeated diagnostic');
        else Promise.reject(reason);
        return 'expected diagnostic';
    },
    get stack() { throw Error('stack getter'); }
};
Promise.reject(reason);
