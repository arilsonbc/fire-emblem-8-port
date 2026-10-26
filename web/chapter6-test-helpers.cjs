const {chapter5x,assert,fs,path}=require('./chapter5x-test-helpers.cjs');
async function chapter6(difficulty=1){const q=await chapter5x(difficulty),scripts=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/event_assets.json')));assert.equal(q.m._fe8_event_call(scripts.find(s=>s.name==='EventScr_Ch5x_EndingScene').address,3),0);q.drain(true);assert.equal(q.m._fe8_event_info(1),-1,'Opening6 '+q.m._fe8_event_info(8)+'/'+q.m._fe8_event_info(9));assert.equal(q.m._fe8_map_info(0),7);return q;}
module.exports={chapter6,assert,fs,path};
