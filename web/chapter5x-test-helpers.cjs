const {chapter5,assert,fs,path}=require('./chapter5-test-helpers.cjs');
async function chapter5x(difficulty=1){const q=await chapter5(difficulty),s=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/event_assets.json')));q.m._fe8_event_call(s.find(s=>s.name==='EventScr_Ch5_EndingScene').address,3);q.drain();assert.equal(q.m._fe8_event_info(1),-1);assert.equal(q.m._fe8_map_info(0),5);return q;}
module.exports={chapter5x,assert,fs,path};
